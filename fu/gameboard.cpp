#include "gameboard.h"
#include <QMouseEvent>
#include <QPainter>

GameBoard::GameBoard(QWidget *parent) : QWidget(parent)
{
    setFixedSize(520, 520);
    myColor = 1;
    reset();
}

GameBoard::~GameBoard()
{
}

void GameBoard::reset()
{
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = 0;
        }
    }
    update();
}

void GameBoard::setMyColor(int color)
{
    myColor = color;
}

int GameBoard::getMyColor() const
{
    return myColor;
}

int GameBoard::getOpponentColor() const
{
    return myColor == 1 ? 2 : 1;
}

int GameBoard::getPieceColor(int x, int y) const
{
    if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
        return board[y][x];
    }
    return 0;
}

void GameBoard::placePiece(int x, int y, int color)
{
    if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
        board[y][x] = color;
        update();
    }
}

int (*GameBoard::getBoard())[BOARD_SIZE]
{
    return board;
}

void GameBoard::getBoardArray(int boardArray[BOARD_SIZE][BOARD_SIZE])
{
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            boardArray[i][j] = board[i][j];
        }
    }
}

int GameBoard::getBoardPos(int pixelPos) const
{
    int offset = 20;
    int cellSize = 35;
    int boardPos = (pixelPos - offset) / cellSize;
    if (boardPos < 0) boardPos = 0;
    if (boardPos >= BOARD_SIZE) boardPos = BOARD_SIZE - 1;
    return boardPos;
}

void GameBoard::mousePressEvent(QMouseEvent *event)
{
    if (!isEnabled()) return;

    // Qt 5/6 兼容：event->x()/event->y() 在 Qt 6 中已被 position().x() 替代
    // 使用 #if 判断版本，或直接用 x()/y()（两者均可用）
    int x = getBoardPos(event->x());
    int y = getBoardPos(event->y());

    if (board[y][x] == 0) {
        board[y][x] = myColor;
        update();
        emit piecePlaced(x, y);
    }
}

void GameBoard::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int offset = 20;
    int cellSize = 35;

    painter.setBrush(QColor(245, 222, 179));
    painter.drawRect(0, 0, width(), height());

    painter.setPen(QColor(100, 100, 100));
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int pos = offset + i * cellSize;
        painter.drawLine(pos, offset, pos, offset + (BOARD_SIZE - 1) * cellSize);
        painter.drawLine(offset, pos, offset + (BOARD_SIZE - 1) * cellSize, pos);
    }

    for (int i = 0; i < BOARD_SIZE; i += 4) {
        for (int j = 0; j < BOARD_SIZE; j += 4) {
            int x = offset + j * cellSize;
            int y = offset + i * cellSize;
            painter.setBrush(QColor(100, 100, 100));
            painter.drawEllipse(x - 3, y - 3, 6, 6);
        }
    }

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != 0) {
                int x = offset + j * cellSize;
                int y = offset + i * cellSize;

                QRadialGradient gradient(x, y, 15);
                if (board[i][j] == 1) {
                    gradient.setColorAt(0, QColor(255, 255, 255));
                    gradient.setColorAt(0.3, QColor(100, 100, 100));
                    gradient.setColorAt(1, QColor(30, 30, 30));
                } else {
                    gradient.setColorAt(0, QColor(255, 255, 255));
                    gradient.setColorAt(0.3, QColor(200, 200, 200));
                    gradient.setColorAt(1, QColor(100, 100, 100));
                }

                painter.setBrush(gradient);
                painter.setPen(QColor(0, 0, 0));
                painter.drawEllipse(x - 15, y - 15, 30, 30);
            }
        }
    }
}