#ifndef AIPLAYER_H
#define AIPLAYER_H

#include <QObject>
#include <QPair>
#include <vector>

const int BOARD_SIZE_AI = 15;

class AIPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AIPlayer(QObject *parent = nullptr);
    void setBoard(int board[BOARD_SIZE_AI][BOARD_SIZE_AI]);
    void setAIColor(int color);
    QPair<int, int> getNextMove();
    static bool checkWin(int board[BOARD_SIZE_AI][BOARD_SIZE_AI], int color);

private:
    int evaluatePosition(int board[BOARD_SIZE_AI][BOARD_SIZE_AI], int row, int col, int color);
    int evaluateLine(int count, int empty, int block);
    std::vector<QPair<int, int>> getCandidates(int board[BOARD_SIZE_AI][BOARD_SIZE_AI]);

    int aiColor;
    int humanColor;
    int currentBoard[BOARD_SIZE_AI][BOARD_SIZE_AI];
};

#endif // AIPLAYER_H