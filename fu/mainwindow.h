#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QRadioButton>
#include "networkmanager.h"
#include "gameboard.h"
#include "aiplayer.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onRegisterClicked();
    void onLoginClicked();
    void onMatchClicked();
    void onSurrenderClicked();
    void onDisconnectClicked();
    void onConnected();
    void onDisconnected();
    void onError(const QString &error);
    void onMessageReceived(const QString &message);
    void onLoginSuccess(const QString &sessionId, int total, int wins, int losses, int draws,
                        const QString &serverUsername = QString());
    void onMatchQueue();
    void onMatchStart(int matchId, const QString &color,
                      const QString &opponentName = QString());
    void onOpponentMove(int x, int y, int moveColor = 0);
    void onYourTurn(int color);
    // 更新状态栏显示当前回合信息
    void updateTurnStatus(bool isMyTurn);
    void onGameEnd(const QString &result);
    void onStatsReceived(int total, int wins, int losses, int draws);
    void onOpponentDisconnected();
    void onPiecePlaced(int x, int y);
    void onAIMove(int x, int y);

private:
    void setupLoginPage();
    void setupGamePage();
    void showLoginPage();
    void showGamePage();
    void updateStats(int total, int wins, int losses, int draws);
    void startAIGame(bool isPlayerFirst);
    void handlePlayerMoveInAI(int x, int y);
    void aiMakeMove();
    void onAIGameEnd(const QString &winner);

    NetworkManager *networkManager;
    AIPlayer *aiPlayer;
    GameBoard *gameBoard;

    QStackedWidget *stackedWidget;

    QWidget *loginPage;
    QLineEdit *serverIpEdit;
    QLineEdit *serverPortEdit;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QPushButton *connectBtn;
    QPushButton *registerBtn;
    QPushButton *loginBtn;
    QRadioButton *onlineModeRadio;
    QRadioButton *aiModeRadio;
    QLabel *statusLabel;

    QWidget *gamePage;
    QLabel *matchInfoLabel;
    QLabel *statsLabel;
    QLabel *gameStatusLabel;
    QTextEdit *logTextEdit;
    QPushButton *matchBtn;
    QPushButton *surrenderBtn;
    QPushButton *disconnectBtn;
    QLabel *modeLabel;

    QString currentSessionId;
    int currentMatchId;
    QString myColor;
    bool isPlaying;
    bool isAIMode;
    int totalGames, wins, losses, draws;
};

#endif // MAINWINDOW_H