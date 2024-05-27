#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOARD_SIZE 9

void printErrorAndExit()
{
    printf("Error\n");
    fflush(stdout);
    exit(1);
}

void printBoard(char *board)
{
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        if (i % 3 == 0 && i != 0)
        {
            printf("\n");
            fflush(stdout);
        }
        printf("%c", board[i]);
        fflush(stdout);
        if (i % 3 != 2)
        {
            printf(" | ");
            fflush(stdout);
        }
    }
    printf("\n");
    printf("\n");
}
int checkWin(char board[BOARD_SIZE])
{
    // Check rows
    for (int i = 0; i < 9; i += 3)
    {
        if (board[i] == board[i + 1] && board[i + 1] == board[i + 2] && board[i] != ' ')
        {
            return board[i] == 'X' ? 1 : 2;
        }
    }

    // Check columns
    for (int i = 0; i < 3; ++i)
    {
        if (board[i] == board[i + 3] && board[i + 3] == board[i + 6] && board[i] != ' ')
        {
            return board[i] == 'X' ? 1 : 2;
        }
    }

    // Check diagonals
    if (board[0] == board[4] && board[4] == board[8] && board[0] != ' ')
    {
        return board[0] == 'X' ? 1 : 2;
    }
    if (board[2] == board[4] && board[4] == board[6] && board[2] != ' ')
    {
        return board[2] == 'X' ? 1 : 2;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2 || strlen(argv[1]) != 9)
    {
        printErrorAndExit();
    }

    char strategy[10];
    strcpy(strategy, argv[1]);

    // Validate strategy
    int count[10] = {0};
    for (int i = 0; i < 9; ++i)
    {
        if (strategy[i] < '1' || strategy[i] > '9' || ++count[strategy[i] - '0'] > 1)
        {
            printErrorAndExit();
        }
    }

    char board[BOARD_SIZE] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int movesMade = 0;
    int playerMove, computerMove;

    while (1)
    {
        // Computer move
        if (movesMade == 0)
        {
            // First move
            computerMove = strategy[0] - '1';
        }
        else if (movesMade == 8)
        {
            // Last move
            for (int i = 0; i < 9; ++i)
            {
                if (board[i] == ' ')
                {
                    computerMove = i;
                    break;
                }
            }
        }
        else
        {
            // Find the highest priority move
            for (int i = 0; i < 9; ++i)
            {
                computerMove = strategy[i] - '1';
                if (board[computerMove] == ' ')
                {

                    break;
                }
            }
        }

        board[computerMove] = 'X';
        movesMade++;

        printf("computer move: %d\n", computerMove+1);
        fflush(stdout);
        printBoard(board);

        if (checkWin(board) == 1)
        {
            printf("I win\n");
            fflush(stdout);
            exit(0);
        }

        if (movesMade == 9)
        {
            printf("DRAW\n");
            fflush(stdout);
            exit(0);
        }

        // Player move
        scanf("%d", &playerMove);
        if (playerMove < 1 || playerMove > 9 || board[playerMove - 1] != ' ')
        {
            printErrorAndExit();
        }

        board[playerMove - 1] = 'O';
        movesMade++;
        printf("Player move: %d\n", playerMove);
        fflush(stdout);
        printBoard(board);

        if (checkWin(board) == 2)
        {
            printf("I lost\n");
            fflush(stdout);
            exit(0);
        }

        if (movesMade == 9)
        {
            printf("DRAW\n");
            fflush(stdout);
            exit(0);
        }
    }

    return 0;
}