#ifndef GAMELOGIC_H
#define GAMELOGIC_H

#include <vector>
#include <array>

const int BOARD_SIZE = 15;
const int WIN_COUNT = 5;

enum PlayerColor {
    EMPTY = 0,
    BLACK = 1,
    WHITE = 2
};

enum GameResult {
    NONE = 0,
    BLACK_WIN = 1,
    WHITE_WIN = 2,
    DRAW = 3
};

struct Move {
    int x;
    int y;
    PlayerColor color;
    int move_number;
};

class GameLogic {
public:
    GameLogic();
    
    void reset();
    bool makeMove(int x, int y, PlayerColor color);
    GameResult checkWin(int x, int y) const;
    bool isValidMove(int x, int y) const;
    PlayerColor getCell(int x, int y) const;
    const std::vector<Move>& getMoveHistory() const;
    int getMoveCount() const;
    bool isBoardFull() const;
    
private:
    std::array<std::array<PlayerColor, BOARD_SIZE>, BOARD_SIZE> board_;
    std::vector<Move> move_history_;
    
    int countInDirection(int x, int y, int dx, int dy, PlayerColor color) const;
};

#endif // GAMELOGIC_H