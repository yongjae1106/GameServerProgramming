#include "../report_01/data.h"

void resetboard()
{
    board[SIZE / 2][SIZE / 2] = 1;
}
void drawboard()
{
    system("cls");

    for (int x = 0; x < SIZE; x++)
        cout << "+---";
    cout << "+\n";

    for (int y = 0; y < SIZE; y++)
    {
        for (int x = 0; x < SIZE; x++)
        {
            cout << "| ";
            if (board[y][x] == 1)
                cout << "@";
            else
                cout << " ";
            cout << " ";
        }
        cout << "|\n";

        for (int x = 0; x < SIZE; x++)
            cout << "+---";
        cout << "+\n";
    }
    cout << "\n press to move (ESC: exit)" << endl;
}

int main()
{
    int x = SIZE / 2; // 4
    int y = SIZE / 2; // 4

    resetboard();
    drawboard();

    while (true)
    {
        int key = _getch();

        // ESC : 27
        if (key == 27)
            break;

        // 특수키 2바이트 : 224
        if (key == 224)
        {
            key = _getch();

            int nx = x;
            int ny = y;

            switch (key)
            {
                case 72: // 상
                {
                    ny--; 
                    break; 
                }
                case 80: // 하
                {
                    ny++;
                    break;
                }
                case 75: // 좌
                {
                    nx--;
                    break;
                }
                case 77: // 우
                {
                    nx++;
                    break;
                }
            }

            // 이동 체크
            if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE)
            {
                board[y][x] = 0;
                x = nx;
                y = ny;
                board[y][x] = 1;
                drawboard();
            }
        }
    }

    return 0;
}