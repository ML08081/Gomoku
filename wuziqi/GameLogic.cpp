#include "GameLogic.h"

GameLogic::GameLogic() {
    reset();
}

void GameLogic::reset() {
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            board_[i][j] = EMPTY;
        }
    }
    move_history_.clear();
}

bool GameLogic::makeMove(int x, int y, PlayerColor color) {
    if (!isValidMove(x, y)) return false;
    if (board_[x][y] != EMPTY) return false;
    
    board_[x][y] = color;
    move_history_.push_back({x, y, color, (int)move_history_.size() + 1});
    return true;
}

bool GameLogic::isValidMove(int x, int y) const {
    return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
}

PlayerColor GameLogic::getCell(int x, int y) const {
    if (!isValidMove(x, y)) return EMPTY;
    return board_[x][y];
}

const std::vector<Move>& GameLogic::getMoveHistory() const {
    return move_history_;
}

int GameLogic::getMoveCount() const {
    return (int)move_history_.size();
}

bool GameLogic::isBoardFull() const {
    return move_history_.size() >= BOARD_SIZE * BOARD_SIZE;
}

GameResult GameLogic::checkWin(int x, int y) const {
    PlayerColor color = board_[x][y];
    if (color == EMPTY) return NONE;
    
    const int directions[4][2] = {
        {1, 0},   // 水平
        {0, 1},   // 垂直
        {1, 1},   // 对角线
        {1, -1}   // 反对角线
    };
    
    for (const auto& dir : directions) {
        int dx = dir[0];
        int dy = dir[1];
        int count = 1;
        
        count += countInDirection(x, y, dx, dy, color);
        count += countInDirection(x, y, -dx, -dy, color);
        
        if (count >= WIN_COUNT) {
            return color == BLACK ? BLACK_WIN : WHITE_WIN;
        }
    }
    
    if (isBoardFull()) {
        return DRAW;
    }
    
    return NONE;
}

int GameLogic::countInDirection(int x, int y, int dx, int dy, PlayerColor color) const {
    int count = 0;
    int nx = x + dx;
    int ny = y + dy;
    
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board_[nx][ny] == color) {
        count++;
        nx += dx;
        ny += dy;
    }
    
    return count;
}