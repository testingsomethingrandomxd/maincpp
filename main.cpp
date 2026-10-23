
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

using namespace std;

struct Cell
{
    char terrain;
    Cell* up;
    Cell* down;
    Cell* left;
    Cell* right;

    Cell(char value)
    {
        terrain = value;
        up = down = left = right = NULL;
    }
};

#ifndef _WIN32
termios oldTerminalSettings;
bool terminalSettingsSaved = false;
#endif

// Enable immediate keyboard input.
// A real interactive terminal is required.
bool enableArrowInput()
{
#ifdef _WIN32
    return true;
#else
    if (!isatty(STDIN_FILENO))
        return false;

    if (tcgetattr(STDIN_FILENO, &oldTerminalSettings) != 0)
        return false;

    terminalSettingsSaved = true;

    termios newSettings = oldTerminalSettings;
    newSettings.c_lflag &= ~(ICANON | ECHO);
    newSettings.c_iflag &= ~(IXON | ICRNL);
    newSettings.c_cc[VMIN] = 0;
    newSettings.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newSettings) != 0)
    {
        terminalSettingsSaved = false;
        return false;
    }

    return true;
#endif
}

// Restore normal terminal input.
void disableArrowInput()
{
#ifndef _WIN32
    if (terminalSettingsSaved)
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTerminalSettings);
#endif
}

// Check whether a key is available.
bool keyAvailable(int waitMilliseconds = 0)
{
#ifdef _WIN32
    if (waitMilliseconds > 0)
        Sleep(waitMilliseconds);

    return _kbhit() != 0;
#else
    fd_set inputSet;
    FD_ZERO(&inputSet);
    FD_SET(STDIN_FILENO, &inputSet);

    timeval timeout;
    timeout.tv_sec = waitMilliseconds / 1000;
    timeout.tv_usec = (waitMilliseconds % 1000) * 1000;

    return select(STDIN_FILENO + 1, &inputSet,
                  NULL, NULL, &timeout) > 0;
#endif
}

// Read one keyboard byte.
int readByte()
{
#ifdef _WIN32
    return _getch();
#else
    unsigned char ch;

    if (read(STDIN_FILENO, &ch, 1) == 1)
        return ch;

    return -1;
#endif
}

// Return U, D, L, R for arrows and 27 for ESC.
// All other keys, including WASD, are ignored.
int readCommand()
{
    if (!keyAvailable())
        return 0;

    int ch = readByte();

    if (ch < 0)
        return 0;

#ifdef _WIN32
    if (ch == 27)
        return 27;

    if (ch == 0 || ch == 224)
    {
        int key = readByte();

        if (key == 72) return 'U';
        if (key == 80) return 'D';
        if (key == 75) return 'L';
        if (key == 77) return 'R';
    }
#else
    if (ch == 27)
    {
        // Arrow keys usually arrive as ESC [ A/B/C/D.
        // Some terminals use ESC O A/B/C/D.

        if (!keyAvailable(100))
            return 27;

        int second = readByte();

        if (second != '[' && second != 'O')
            return 27;

        if (!keyAvailable(100))
            return 0;

        int third = readByte();

        if (third == 'A') return 'U';
        if (third == 'B') return 'D';
        if (third == 'C') return 'R';
        if (third == 'D') return 'L';
    }
#endif

    return 0;
}

// Find a cell by starting at the root.
Cell* cellAt(Cell* root, int row, int column)
{
    if (root == NULL || row < 0 || column < 0)
        return NULL;

    Cell* current = root;

    for (int i = 0; i < row && current != NULL; i++)
        current = current->down;

    for (int j = 0; j < column && current != NULL; j++)
        current = current->right;

    return current;
}

// Create a dynamic N x N linked mesh.
Cell* createMesh(int size)
{
    Cell* root = NULL;

    for (int row = 0; row < size; row++)
    {
        for (int column = 0; column < size; column++)
        {
            char terrain = '.';

            if (row == 0 || column == 0 ||
                row == size - 1 || column == size - 1)
            {
                terrain = '#';
            }
            else if (row == 2 && column == 2)
            {
                terrain = '#';
            }
            else if (row == 3 && column == 2)
            {
                terrain = 'G';
            }

            Cell* node = new Cell(terrain);

            if (row == 0 && column == 0)
                root = node;

            // Connect left and right cells.
            if (column > 0)
            {
                Cell* leftCell =
                    cellAt(root, row, column - 1);

                leftCell->right = node;
                node->left = leftCell;
            }

            // Connect upper and lower cells.
            if (row > 0)
            {
                Cell* upperCell =
                    cellAt(root, row - 1, column);

                upperCell->down = node;
                node->up = upperCell;
            }
        }
    }

    return root;
}

// Count the remaining dots.
int countDots(Cell* root, int size)
{
    int count = 0;

    for (int row = 0; row < size; row++)
    {
        for (int column = 0; column < size; column++)
        {
            Cell* cell = cellAt(root, row, column);

            if (cell != NULL && cell->terrain == '.')
                count++;
        }
    }

    return count;
}

// Display the direction of movement.
const char* velocityName(int dr, int dc)
{
    if (dr == 0 && dc == 1)
        return "EAST [ARROW_RIGHT]";

    if (dr == 0 && dc == -1)
        return "WEST [ARROW_LEFT]";

    if (dr == -1 && dc == 0)
        return "NORTH [ARROW_UP]";

    if (dr == 1 && dc == 0)
        return "SOUTH [ARROW_DOWN]";

    return "STOPPED";
}

// Clear the console screen.
void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    cout << "\033[2J\033[H";
#endif
}

// Draw the board by traversing from the root.
void drawBoard(Cell* root, int size,
               int playerRow, int playerColumn,
               char sprite, int score, int dr, int dc)
{
    clearScreen();

    cout << "PAC-MAN: ORTHOGONAL LINKED MESH\n";
    cout << "Score: " << score
         << " | Velocity: " << velocityName(dr, dc)
         << '\n';

    for (int row = 0; row < size; row++)
    {
        for (int column = 0; column < size; column++)
        {
            if (row == playerRow && column == playerColumn)
                cout << sprite;
            else
                cout << cellAt(root, row, column)->terrain;

            cout << ' ';
        }

        cout << '\n';
    }

    cout << "Use ARROW KEYS only. Press ESC to quit.\n";
    cout.flush();
}

// Free all dynamically created nodes.
void deleteMesh(Cell* root)
{
    Cell* rowStart = root;

    while (rowStart != NULL)
    {
        Cell* nextRow = rowStart->down;
        Cell* current = rowStart;

        while (current != NULL)
        {
            Cell* nextCell = current->right;

            delete current;
            current = nextCell;
        }

        rowStart = nextRow;
    }
}

int main()
{
    const int SIZE = 5;
    const int MOVE_DELAY_MS = 800;

    // The single master pointer to the mesh.
    Cell* root = createMesh(SIZE);

    int playerRow = 1;
    int playerColumn = 1;
    int score = 0;

    // Pac-Man starts moving right automatically.
    int dr = 0;
    int dc = 1;
    char sprite = '>';

    cellAt(root, playerRow, playerColumn)->terrain = ' ';

    int dotsLeft = countDots(root, SIZE);

    if (!enableArrowInput())
    {
        cout << "This program needs an interactive terminal.\n";
        cout << "Run it in the IDE terminal, not an input box.\n";

        deleteMesh(root);
        return 1;
    }

    drawBoard(root, SIZE, playerRow, playerColumn,
              sprite, score, dr, dc);

    bool running = true;

    auto nextMove = chrono::steady_clock::now()
                  + chrono::milliseconds(MOVE_DELAY_MS);

    while (running && dotsLeft > 0)
    {
        int command = readCommand();

        if (command == 27)
        {
            running = false;
            break;
        }

        int newDr = dr;
        int newDc = dc;
        char newSprite = sprite;

        if (command == 'U')
        {
            newDr = -1;
            newDc = 0;
            newSprite = '^';
        }
        else if (command == 'D')
        {
            newDr = 1;
            newDc = 0;
            newSprite = 'v';
        }
        else if (command == 'L')
        {
            newDr = 0;
            newDc = -1;
            newSprite = '<';
        }
        else if (command == 'R')
        {
            newDr = 0;
            newDc = 1;
            newSprite = '>';
        }

        // Change direction only if the next cell is free.
        if (command == 'U' || command == 'D' ||
            command == 'L' || command == 'R')
        {
            Cell* turnCell =
                cellAt(root, playerRow + newDr,
                       playerColumn + newDc);

            if (turnCell == NULL ||
                turnCell->terrain == '#' ||
                turnCell->terrain == 'G')
            {
                dr = 0;
                dc = 0;
            }
            else
            {
                dr = newDr;
                dc = newDc;
                sprite = newSprite;
            }

            drawBoard(root, SIZE, playerRow, playerColumn,
                      sprite, score, dr, dc);

            nextMove = chrono::steady_clock::now()
                     + chrono::milliseconds(MOVE_DELAY_MS);
        }

        auto now = chrono::steady_clock::now();

        // Move automatically at regular intervals.
        if (now >= nextMove)
        {
            if (dr != 0 || dc != 0)
            {
                Cell* destination =
                    cellAt(root, playerRow + dr,
                           playerColumn + dc);

                // Stop on a wall or ghost collision.
                if (destination == NULL ||
                    destination->terrain == '#' ||
                    destination->terrain == 'G')
                {
                    dr = 0;
                    dc = 0;
                }
                else
                {
                    playerRow += dr;
                    playerColumn += dc;

                    if (destination->terrain == '.')
                    {
                        destination->terrain = ' ';
                        score++;
                        dotsLeft--;
                    }
                }
            }

            drawBoard(root, SIZE, playerRow, playerColumn,
                      sprite, score, dr, dc);

            nextMove = chrono::steady_clock::now()
                     + chrono::milliseconds(MOVE_DELAY_MS);
        }

        this_thread::sleep_for(chrono::milliseconds(5));
    }

    disableArrowInput();

    if (dotsLeft == 0)
        cout << "All dots collected! Final score: "
             << score << '\n';
    else
        cout << "Game ended. Final score: "
             << score << '\n';

    deleteMesh(root);
    root = NULL;

    return 0;
}

