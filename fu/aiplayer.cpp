#include "aiplayer.h"
#include <QDebug>
#include <algorithm>
#include <random>

AIPlayer::AIPlayer(QObject *parent) : QObject(parent)
{
    aiColor = 2;
    humanColor = 1;
}

void AIPlayer::setBoard(int board[BOARD_SIZE_AI][BOARD_SIZE_AI])
{
    for (int i = 0; i < BOARD_SIZE_AI; i++) {
        for (int j = 0; j < BOARD_SIZE_AI; j++) {
            currentBoard[i][j] = board[i][j];
        }
    }
}

void AIPlayer::setAIColor(int color)
{
    aiColor = color;
    humanColor = (color == 1) ? 2 : 1;
}

QPair<int, int> AIPlayer::getNextMove()
{
    std::vector<QPair<int, int>> candidates = getCandidates(currentBoard);

    if (candidates.empty()) {
        return qMakePair(7, 7);
    }

    int bestScore = -1000000;
    QPair<int, int> bestMove = candidates[0];

    for (const auto &move : candidates) {
        int x = move.first;
        int y = move.second;

        currentBoard[y][x] = aiColor;
        int attackScore = evaluatePosition(currentBoard, y, x, aiColor);
        currentBoard[y][x] = 0;

        currentBoard[y][x] = humanColor;
        int defenseScore = evaluatePosition(currentBoard, y, x, humanColor);
        currentBoard[y][x] = 0;

        int totalScore = attackScore * 2 + defenseScore * 1;

        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestMove = move;
        }
    }

    return bestMove;
}

std::vector<QPair<int, int>> AIPlayer::getCandidates(int board[BOARD_SIZE_AI][BOARD_SIZE_AI])
{
    std::vector<QPair<int, int>> candidates;
    bool hasAnyPiece = false;

    for (int i = 0; i < BOARD_SIZE_AI; i++) {
        for (int j = 0; j < BOARD_SIZE_AI; j++) {
            if (board[i][j] != 0) {
                hasAnyPiece = true;
                break;
            }
        }
        if (hasAnyPiece) break;
    }

    if (!hasAnyPiece) {
        candidates.push_back(qMakePair(7, 7));
        return candidates;
    }

    for (int i = 0; i < BOARD_SIZE_AI; i++) {
        for (int j = 0; j < BOARD_SIZE_AI; j++) {
            if (board[i][j] == 0) {
                bool hasNeighbor = false;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int ny = i + dy;
                        int nx = j + dx;
                        if (ny >= 0 && ny < BOARD_SIZE_AI && nx >= 0 && nx < BOARD_SIZE_AI) {
                            if (board[ny][nx] != 0) {
                                hasNeighbor = true;
                                break;
                            }
                        }
                    }
                    if (hasNeighbor) break;
                }
                if (hasNeighbor) {
                    candidates.push_back(qMakePair(j, i));
                }
            }
        }
    }

    if (candidates.size() > 30) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(candidates.begin(), candidates.end(), g);
        candidates.resize(30);
    }

    return candidates;
}

int AIPlayer::evaluatePosition(int board[BOARD_SIZE_AI][BOARD_SIZE_AI], int row, int col, int color)
{
    int score = 0;
    const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        int count = 1;
        int empty = 0;
        int block = 0;

        for (int i = 1; i < 6; i++) {
            int nx = col + dx * i;
            int ny = row + dy * i;
            if (ny >= 0 && ny < BOARD_SIZE_AI && nx >= 0 && nx < BOARD_SIZE_AI) {
                if (board[ny][nx] == color) {
                    count++;
                } else if (board[ny][nx] == 0) {
                    if (empty == 0) empty = 1;
                    else { block++; break; }
                } else {
                    block++;
                    break;
                }
            } else {
                block++;
                break;
            }
        }

        for (int i = 1; i < 6; i++) {
            int nx = col - dx * i;
            int ny = row - dy * i;
            if (ny >= 0 && ny < BOARD_SIZE_AI && nx >= 0 && nx < BOARD_SIZE_AI) {
                if (board[ny][nx] == color) {
                    count++;
                } else if (board[ny][nx] == 0) {
                    if (empty == 0) empty = 1;
                    else { block++; break; }
                } else {
                    block++;
                    break;
                }
            } else {
                block++;
                break;
            }
        }

        score += evaluateLine(count, empty, block);
    }

    return score;
}

int AIPlayer::evaluateLine(int count, int empty, int block)
{
    if (block == 2) return 0;

    if (count >= 5) return 100000;
    if (count == 4) {
        if (empty == 1) return 10000;
        if (empty == 0) return 1000;
    }
    if (count == 3) {
        if (empty == 2) return 5000;
        if (empty == 1) return 500;
        if (empty == 0) return 100;
    }
    if (count == 2) {
        if (empty == 2) return 200;
        if (empty == 1) return 50;
        if (empty == 0) return 10;
    }
    if (count == 1) {
        if (empty >= 1) return 5;
    }

    return count;
}

bool AIPlayer::checkWin(int board[BOARD_SIZE_AI][BOARD_SIZE_AI], int color)
{
    for (int i = 0; i < BOARD_SIZE_AI; i++) {
        for (int j = 0; j < BOARD_SIZE_AI; j++) {
            if (board[i][j] != color) continue;

            if (j <= BOARD_SIZE_AI - 5) {
                bool win = true;
                for (int k = 0; k < 5; k++) {
                    if (board[i][j + k] != color) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }

            if (i <= BOARD_SIZE_AI - 5) {
                bool win = true;
                for (int k = 0; k < 5; k++) {
                    if (board[i + k][j] != color) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }

            if (i <= BOARD_SIZE_AI - 5 && j <= BOARD_SIZE_AI - 5) {
                bool win = true;
                for (int k = 0; k < 5; k++) {
                    if (board[i + k][j + k] != color) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }

            if (i <= BOARD_SIZE_AI - 5 && j >= 4) {
                bool win = true;
                for (int k = 0; k < 5; k++) {
                    if (board[i + k][j - k] != color) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }
        }
    }
    return false;
}