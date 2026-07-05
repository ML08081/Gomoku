#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>

const int BOARD_SIZE = 15;

class GameBoard : public QWidget
{
    Q_OBJECT

public:
    explicit GameBoard(QWidget *parent = nullptr);
    ~GameBoard();

    void reset();
    void placePiece(int x, int y, int color);
    void setMyColor(int color);
    int getMyColor() const;
    int getOpponentColor() const;
    // 获取当前棋盘某位置的棋子颜色（0=空, 1=黑, 2=白）
    int getPieceColor(int x, int y) const;
    int (*getBoard())[BOARD_SIZE];
    void getBoardArray(int board[BOARD_SIZE][BOARD_SIZE]);

signals:
    void piecePlaced(int x, int y);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int getBoardPos(int pixelPos) const;

    int board[BOARD_SIZE][BOARD_SIZE];
    int myColor;
};

#endif // GAMEBOARD_H