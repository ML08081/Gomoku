#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(new NetworkManager(this))
    , aiPlayer(new AIPlayer(this))
    , currentMatchId(-1)
    , isPlaying(false)
    , isAIMode(false)
    , totalGames(0), wins(0), losses(0), draws(0)
{
    setWindowTitle(QString::fromUtf8("五子棋游戏"));
    resize(950, 750);
    setStyleSheet("QMainWindow { background-color: #f5f5f5; }");

    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    setupLoginPage();
    setupGamePage();

    showLoginPage();

    connect(networkManager, &NetworkManager::connected, this, &MainWindow::onConnected);
    connect(networkManager, &NetworkManager::disconnected, this, &MainWindow::onDisconnected);
    connect(networkManager, &NetworkManager::errorOccurred, this, &MainWindow::onError);
    connect(networkManager, &NetworkManager::messageReceived, this, &MainWindow::onMessageReceived);

    connect(gameBoard, &GameBoard::piecePlaced, this, &MainWindow::onPiecePlaced);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupLoginPage()
{
    loginPage = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(loginPage);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(50, 50, 50, 50);

    QLabel *titleLabel = new QLabel(QString::fromUtf8("五子棋游戏"));
    titleLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: #333;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QFrame *formFrame = new QFrame;
    formFrame->setStyleSheet("QFrame { background-color: white; border-radius: 10px; padding: 20px; }");
    QVBoxLayout *formLayout = new QVBoxLayout(formFrame);
    formLayout->setSpacing(15);

    QGridLayout *inputLayout = new QGridLayout;
    inputLayout->setSpacing(10);

    QLabel *ipLabel = new QLabel(QString::fromUtf8("服务器地址:"));
    ipLabel->setStyleSheet("font-size: 14px; color: #666;");
    serverIpEdit = new QLineEdit("192.168.118.137");
    serverIpEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; }");
    inputLayout->addWidget(ipLabel, 0, 0);
    inputLayout->addWidget(serverIpEdit, 0, 1);

    QLabel *portLabel = new QLabel(QString::fromUtf8("端口:"));
    portLabel->setStyleSheet("font-size: 14px; color: #666;");
    serverPortEdit = new QLineEdit("8888");
    serverPortEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; }");
    inputLayout->addWidget(portLabel, 1, 0);
    inputLayout->addWidget(serverPortEdit, 1, 1);

    QLabel *userLabel = new QLabel(QString::fromUtf8("用户名:"));
    userLabel->setStyleSheet("font-size: 14px; color: #666;");
    usernameEdit = new QLineEdit();
    usernameEdit->setPlaceholderText(QString::fromUtf8("请输入用户名"));
    usernameEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; }");
    inputLayout->addWidget(userLabel, 2, 0);
    inputLayout->addWidget(usernameEdit, 2, 1);

    QLabel *passLabel = new QLabel(QString::fromUtf8("密码:"));
    passLabel->setStyleSheet("font-size: 14px; color: #666;");
    passwordEdit = new QLineEdit();
    passwordEdit->setPlaceholderText(QString::fromUtf8("请输入密码"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; }");
    inputLayout->addWidget(passLabel, 3, 0);
    inputLayout->addWidget(passwordEdit, 3, 1);

    formLayout->addLayout(inputLayout);

    QFrame *modeFrame = new QFrame;
    modeFrame->setStyleSheet("QFrame { background-color: #f0f0f0; border-radius: 8px; padding: 15px; margin-top: 10px; }");
    QVBoxLayout *modeLayout = new QVBoxLayout(modeFrame);

    QLabel *modeTitle = new QLabel(QString::fromUtf8("游戏模式"));
    modeTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 10px;");
    modeLayout->addWidget(modeTitle);

    QHBoxLayout *radioLayout = new QHBoxLayout;
    onlineModeRadio = new QRadioButton(QString::fromUtf8("在线对战"));
    onlineModeRadio->setChecked(true);
    onlineModeRadio->setStyleSheet("QRadioButton { font-size: 14px; }");
    aiModeRadio = new QRadioButton(QString::fromUtf8("人机对战"));
    aiModeRadio->setStyleSheet("QRadioButton { font-size: 14px; }");
    radioLayout->addWidget(onlineModeRadio);
    radioLayout->addWidget(aiModeRadio);
    modeLayout->addLayout(radioLayout);

    formLayout->addWidget(modeFrame);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(10);

    connectBtn = new QPushButton(QString::fromUtf8("连接服务器"));
    connectBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-size: 14px; } QPushButton:hover { background-color: #45a049; }");
    registerBtn = new QPushButton(QString::fromUtf8("注册"));
    registerBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-size: 14px; } QPushButton:hover { background-color: #1976D2; }");
    loginBtn = new QPushButton(QString::fromUtf8("登录"));
    loginBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-size: 14px; } QPushButton:hover { background-color: #F57C00; }");

    btnLayout->addWidget(connectBtn);
    btnLayout->addWidget(registerBtn);
    btnLayout->addWidget(loginBtn);

    formLayout->addLayout(btnLayout);
    mainLayout->addWidget(formFrame, 1);

    statusLabel = new QLabel(QString::fromUtf8("等待连接..."));
    statusLabel->setStyleSheet("font-size: 12px; color: #999;");
    statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel);

    stackedWidget->addWidget(loginPage);

    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(registerBtn, &QPushButton::clicked, this, &MainWindow::onRegisterClicked);
    connect(loginBtn, &QPushButton::clicked, this, &MainWindow::onLoginClicked);

    connect(onlineModeRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            serverIpEdit->setEnabled(true);
            serverPortEdit->setEnabled(true);
        }
    });

    connect(aiModeRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            serverIpEdit->setEnabled(false);
            serverPortEdit->setEnabled(false);
        }
    });
}

void MainWindow::setupGamePage()
{
    gamePage = new QWidget;
    QHBoxLayout *mainLayout = new QHBoxLayout(gamePage);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(15);

    QFrame *boardFrame = new QFrame;
    boardFrame->setStyleSheet("QFrame { background-color: white; border-radius: 10px; padding: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); }");
    QVBoxLayout *boardLayout = new QVBoxLayout(boardFrame);

    gameBoard = new GameBoard;
    gameBoard->setEnabled(false);
    boardLayout->addWidget(gameBoard);
    leftLayout->addWidget(boardFrame);

    QFrame *infoFrame = new QFrame;
    infoFrame->setStyleSheet("QFrame { background-color: white; border-radius: 10px; padding: 15px; }");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setSpacing(10);

    matchInfoLabel = new QLabel(QString::fromUtf8("等待连接..."));
    matchInfoLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #333;");
    matchInfoLabel->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(matchInfoLabel);

    modeLabel = new QLabel(QString::fromUtf8("模式: 在线"));
    modeLabel->setStyleSheet("font-size: 14px; color: #666;");
    modeLabel->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(modeLabel);

    QFrame *statsFrame = new QFrame;
    statsFrame->setStyleSheet("QFrame { background-color: #e8f5e9; border-radius: 8px; padding: 15px; }");
    QVBoxLayout *statsLayout = new QVBoxLayout(statsFrame);

    QLabel *statsTitle = new QLabel(QString::fromUtf8("战绩统计"));
    statsTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #2E7D32; margin-bottom: 10px;");
    statsLayout->addWidget(statsTitle);

    statsLabel = new QLabel(QString::fromUtf8("总场次: 0\n胜: 0 负: 0 平: 0"));
    statsLabel->setStyleSheet("font-size: 13px; color: #555;");
    statsLayout->addWidget(statsLabel);
    infoLayout->addWidget(statsFrame);

    QFrame *statusFrame = new QFrame;
    statusFrame->setStyleSheet("QFrame { background-color: #fff3e0; border-radius: 8px; padding: 15px; }");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusFrame);

    QLabel *statusTitle = new QLabel(QString::fromUtf8("当前状态"));
    statusTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #E65100; margin-bottom: 5px;");
    statusLayout->addWidget(statusTitle);

    gameStatusLabel = new QLabel(QString::fromUtf8("未开始游戏"));
    gameStatusLabel->setStyleSheet("font-size: 16px; color: #E65100;");
    statusLayout->addWidget(gameStatusLabel);
    infoLayout->addWidget(statusFrame);

    leftLayout->addWidget(infoFrame);
    mainLayout->addLayout(leftLayout);

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(15);

    QFrame *controlFrame = new QFrame;
    controlFrame->setStyleSheet("QFrame { background-color: white; border-radius: 10px; padding: 15px; }");
    QVBoxLayout *controlLayout = new QVBoxLayout(controlFrame);
    controlLayout->setSpacing(10);

    matchBtn = new QPushButton(QString::fromUtf8("开始匹配"));
    matchBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 12px; border: none; border-radius: 5px; font-size: 16px; font-weight: bold; } QPushButton:hover { background-color: #45a049; } QPushButton:disabled { background-color: #ccc; }");
    controlLayout->addWidget(matchBtn);

    surrenderBtn = new QPushButton(QString::fromUtf8("认输"));
    surrenderBtn->setEnabled(false);
    surrenderBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 10px; border: none; border-radius: 5px; font-size: 14px; } QPushButton:hover { background-color: #d32f2f; } QPushButton:disabled { background-color: #ccc; }");
    controlLayout->addWidget(surrenderBtn);

    disconnectBtn = new QPushButton(QString::fromUtf8("返回登录"));
    disconnectBtn->setStyleSheet("QPushButton { background-color: #9E9E9E; color: white; padding: 10px; border: none; border-radius: 5px; font-size: 14px; } QPushButton:hover { background-color: #757575; }");
    controlLayout->addWidget(disconnectBtn);
    rightLayout->addWidget(controlFrame);

    QFrame *logFrame = new QFrame;
    logFrame->setStyleSheet("QFrame { background-color: white; border-radius: 10px; padding: 15px; }");
    QVBoxLayout *logLayout = new QVBoxLayout(logFrame);

    QLabel *logTitle = new QLabel(QString::fromUtf8("游戏日志"));
    logTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 10px;");
    logLayout->addWidget(logTitle);

    logTextEdit = new QTextEdit();
    logTextEdit->setReadOnly(true);
    logTextEdit->setMaximumHeight(200);
    logTextEdit->setStyleSheet("QTextEdit { background-color: #f8f9fa; border: 1px solid #eee; border-radius: 5px; padding: 10px; font-size: 12px; }");
    logLayout->addWidget(logTextEdit);
    rightLayout->addWidget(logFrame);

    QFrame *rulesFrame = new QFrame;
    rulesFrame->setStyleSheet("QFrame { background-color: #e3f2fd; border-radius: 10px; padding: 15px; }");
    QVBoxLayout *rulesLayout = new QVBoxLayout(rulesFrame);

    QLabel *rulesTitle = new QLabel(QString::fromUtf8("游戏规则"));
    rulesTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #1565C0; margin-bottom: 10px;");
    rulesLayout->addWidget(rulesTitle);

    QLabel *rulesText = new QLabel(QString::fromUtf8("• 黑方先行\n• 五子连珠获胜\n• 横、竖、斜均可\n• 棋盘为15×15"));
    rulesText->setStyleSheet("font-size: 12px; color: #1976D2;");
    rulesLayout->addWidget(rulesText);
    rightLayout->addWidget(rulesFrame);

    rightLayout->addStretch();
    mainLayout->addLayout(rightLayout);

    stackedWidget->addWidget(gamePage);

    connect(matchBtn, &QPushButton::clicked, this, &MainWindow::onMatchClicked);
    connect(surrenderBtn, &QPushButton::clicked, this, &MainWindow::onSurrenderClicked);
    connect(disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
}

void MainWindow::showLoginPage()
{
    stackedWidget->setCurrentWidget(loginPage);
}

void MainWindow::showGamePage()
{
    stackedWidget->setCurrentWidget(gamePage);
}

void MainWindow::updateStats(int total, int wins, int losses, int draws)
{
    this->totalGames = total;
    this->wins = wins;
    this->losses = losses;
    this->draws = draws;

    statsLabel->setText(QString::fromUtf8("总场次: %1\n胜: %2 负: %3 平: %4")
                         .arg(total).arg(wins).arg(losses).arg(draws));
}

void MainWindow::onConnectClicked()
{
    QString ip   = serverIpEdit->text().trimmed();
    int     port = serverPortEdit->text().toInt();

    if (ip.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请输入服务器地址"));
        return;
    }
    if (port <= 0 || port > 65535) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("端口号无效（1-65535）"));
        return;
    }

    connectBtn->setEnabled(false);
    statusLabel->setText(QString::fromUtf8("正在连接..."));
    logTextEdit->append(QString::fromUtf8("[连接] 正在连接到 %1:%2").arg(ip).arg(port));
    networkManager->connectToServer(ip, static_cast<quint16>(port));
}

void MainWindow::onRegisterClicked()
{
    if (aiModeRadio->isChecked()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("人机模式不需要注册"));
        return;
    }

    QString username = usernameEdit->text();
    QString password = passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("警告"), QString::fromUtf8("用户名和密码不能为空"));
        return;
    }

    QString logMsg = QString::fromUtf8("[注册] 尝试注册用户: %1").arg(username);
    logTextEdit->append(logMsg);
    networkManager->sendRegister(username, password);
}

void MainWindow::onLoginClicked()
{
    QString username = usernameEdit->text();
    QString password = passwordEdit->text();

    if (aiModeRadio->isChecked()) {
        isAIMode = true;
        modeLabel->setText(QString::fromUtf8("模式: 人机对战"));
        showGamePage();
        matchInfoLabel->setText(QString::fromUtf8("人机模式"));
        gameStatusLabel->setText(QString::fromUtf8("人机模式：执黑先行"));
        logTextEdit->append(QString::fromUtf8("[登录] 进入人机对战模式"));
        return;
    }

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("警告"), QString::fromUtf8("用户名和密码不能为空"));
        return;
    }

    QString logMsg = QString::fromUtf8("[登录] 尝试登录用户: %1").arg(username);
    logTextEdit->append(logMsg);
    networkManager->sendLogin(username, password);
}

void MainWindow::onMatchClicked()
{
    if (isPlaying) {
        return;
    }

    if (isAIMode) {
        startAIGame(true);
    } else {
        networkManager->sendMatch();
        gameStatusLabel->setText(QString::fromUtf8("正在匹配中..."));
        matchBtn->setEnabled(false);
        logTextEdit->append(QString::fromUtf8("[匹配] 进入匹配队列"));
    }
}

void MainWindow::startAIGame(bool isPlayerFirst)
{
    isPlaying = true;
    gameBoard->reset();
    surrenderBtn->setEnabled(true);
    matchBtn->setEnabled(false);

    if (isPlayerFirst) {
        myColor = "BLACK";
        gameBoard->setMyColor(1);
        matchInfoLabel->setText(QString::fromUtf8("执黑先行"));
        gameStatusLabel->setText(QString::fromUtf8("轮到你落子（黑棋）"));
        logTextEdit->append(QString::fromUtf8("[游戏] 你执黑棋，请落子"));
        gameBoard->setEnabled(true);
    } else {
        myColor = "WHITE";
        gameBoard->setMyColor(2);
        aiPlayer->setAIColor(1);
        matchInfoLabel->setText(QString::fromUtf8("执白后行"));
        gameStatusLabel->setText(QString::fromUtf8("等待AI落子..."));
        logTextEdit->append(QString::fromUtf8("[游戏] 你执白棋，AI执黑棋"));
        gameBoard->setEnabled(false);
        QTimer::singleShot(500, this, &MainWindow::aiMakeMove);
    }
}

void MainWindow::onPiecePlaced(int x, int y)
{
    if (isPlaying && !isAIMode) {
        // 发送落子到服务端
        networkManager->sendMove(x, y);
        // 本地立即禁用棋盘，等待服务端确认（防止重复落子）
        gameBoard->setEnabled(false);
        updateTurnStatus(false);
        logTextEdit->append(QString::fromUtf8("[游戏] 你发送落子: (%1,%2)").arg(x).arg(y));
    } else if (isPlaying && isAIMode) {
        handlePlayerMoveInAI(x, y);
    }
}

void MainWindow::handlePlayerMoveInAI(int x, int y)
{
    QString logMsg = QString::fromUtf8("[游戏] 你落子: (%1,%2)").arg(x).arg(y);
    logTextEdit->append(logMsg);

    int board[15][15];
    gameBoard->getBoardArray(board);

    if (AIPlayer::checkWin(board, gameBoard->getMyColor())) {
        onAIGameEnd(myColor);
        return;
    }

    gameBoard->setEnabled(false);
    gameStatusLabel->setText(QString::fromUtf8("等待AI响应..."));

    QTimer::singleShot(300, this, &MainWindow::aiMakeMove);
}

void MainWindow::aiMakeMove()
{
    if (!isPlaying) return;

    int board[15][15];
    gameBoard->getBoardArray(board);
    aiPlayer->setBoard(board);

    QPair<int, int> move = aiPlayer->getNextMove();
    int aiX = move.first;
    int aiY = move.second;

    int aiColor = (myColor == "BLACK") ? 2 : 1;
    gameBoard->placePiece(aiX, aiY, aiColor);

    QString logMsg = QString::fromUtf8("[游戏] AI落子: (%1,%2)").arg(aiX).arg(aiY);
    logTextEdit->append(logMsg);

    int currentBoard[15][15];
    gameBoard->getBoardArray(currentBoard);
    if (AIPlayer::checkWin(currentBoard, aiColor)) {
        onAIGameEnd((aiColor == 1) ? "BLACK" : "WHITE");
        return;
    }

    gameStatusLabel->setText(QString::fromUtf8("轮到你落子"));
    gameBoard->setEnabled(true);
}

void MainWindow::onAIGameEnd(const QString &winner)
{
    isPlaying = false;
    gameBoard->setEnabled(false);
    surrenderBtn->setEnabled(false);
    matchBtn->setEnabled(true);

    QString resultText;
    if (winner == myColor) {
        resultText = QString::fromUtf8("你赢了！");
        wins++;
        logTextEdit->append(QString::fromUtf8("[游戏] 你获胜了！"));
    } else {
        resultText = QString::fromUtf8("AI赢了！");
        losses++;
        logTextEdit->append(QString::fromUtf8("[游戏] AI获胜了"));
    }
    totalGames++;

    gameStatusLabel->setText(QString::fromUtf8("游戏结束: ") + resultText);
    updateStats(totalGames, wins, losses, draws);

    QMessageBox::information(this, QString::fromUtf8("游戏结束"), resultText);
}

void MainWindow::onSurrenderClicked()
{
    if (!isPlaying) return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QString::fromUtf8("确认认输"), QString::fromUtf8("确定要认输吗?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (isAIMode) {
            losses++;
            totalGames++;
            updateStats(totalGames, wins, losses, draws);
            isPlaying = false;
            gameBoard->setEnabled(false);
            surrenderBtn->setEnabled(false);
            matchBtn->setEnabled(true);
            gameStatusLabel->setText(QString::fromUtf8("你认输了"));
            logTextEdit->append(QString::fromUtf8("[游戏] 你认输了"));
            QMessageBox::information(this, QString::fromUtf8("游戏结束"), QString::fromUtf8("你认输了！"));
        } else {
            networkManager->sendSurrender();
            logTextEdit->append(QString::fromUtf8("[游戏] 已认输"));
        }
    }
}

void MainWindow::onDisconnectClicked()
{
    isPlaying = false;
    isAIMode = false;
    modeLabel->setText(QString::fromUtf8("模式: 在线"));
    networkManager->disconnectFromServer();
    logTextEdit->append(QString::fromUtf8("[断开] 已断开连接"));
    showLoginPage();
}

void MainWindow::onConnected()
{
    connectBtn->setEnabled(true);
    statusLabel->setText(QString::fromUtf8("已连接服务器"));
    matchInfoLabel->setText(QString::fromUtf8("已连接"));
    logTextEdit->append(QString::fromUtf8("[连接] 成功连接到服务器"));
}

void MainWindow::onDisconnected()
{
    isPlaying = false;
    gameBoard->reset();
    gameBoard->setEnabled(false);
    matchBtn->setEnabled(true);
    surrenderBtn->setEnabled(false);
    connectBtn->setEnabled(true);
    gameStatusLabel->setText(QString::fromUtf8("已断开连接"));
    matchInfoLabel->setText(QString::fromUtf8("未连接"));
    logTextEdit->append(QString::fromUtf8("[断开] 与服务器断开连接"));
    showLoginPage();
}

void MainWindow::onError(const QString &error)
{
    connectBtn->setEnabled(true);
    QMessageBox::critical(this, QString::fromUtf8("错误"), error);
    logTextEdit->append(QString::fromUtf8("[错误] ") + error);
}

void MainWindow::onMessageReceived(const QString &message)
{
    qDebug() << "Received:" << message;
    logTextEdit->append(QString::fromUtf8("[收到] ") + message);

    QStringList parts = message.split(' ');
    if (parts.isEmpty()) return;

    const QString& command = parts[0];

    if (command == "REGISTER_OK") {
        // 注册成功
        QMessageBox::information(this,
            QString::fromUtf8("注册成功"),
            QString::fromUtf8("注册成功！请使用刚才的账号密码登录。"));
        logTextEdit->append(QString::fromUtf8("[注册] 注册成功"));

    } else if (command == "LOGIN_OK") {
        // 格式：LOGIN_OK <session_id> <total> <wins> <losses> <draws> <username>
        if (parts.size() >= 6) {
            currentSessionId = parts[1];
            totalGames = parts[2].toInt();
            wins       = parts[3].toInt();
            losses     = parts[4].toInt();
            draws      = parts[5].toInt();
            // username 字段（可选，兼容旧格式）
            QString serverUsername = (parts.size() >= 7) ? parts[6] : usernameEdit->text();
            onLoginSuccess(currentSessionId, totalGames, wins, losses, draws, serverUsername);
        }

    } else if (command == "MATCH_QUEUE") {
        // 进入匹配队列
        onMatchQueue();

    } else if (command == "MATCH_START") {
        // 格式：MATCH_START <match_id> <color> [opponent_name]
        if (parts.size() >= 3) {
            currentMatchId = parts[1].toInt();
            myColor        = parts[2];
            QString opponentName = (parts.size() >= 4) ? parts[3] : QString::fromUtf8("对手");
            onMatchStart(currentMatchId, myColor, opponentName);
        }

    } else if (command == "MOVE") {
        // 格式：MOVE <x> <y> [color]  (color: 1=黑, 2=白)
        if (parts.size() >= 3) {
            int x = parts[1].toInt();
            int y = parts[2].toInt();
            int moveColor = (parts.size() >= 4) ? parts[3].toInt() : 0;
            onOpponentMove(x, y, moveColor);
        }

    } else if (command == "TURN") {
        // 格式：TURN <color>
        if (parts.size() >= 2) {
            onYourTurn(parts[1].toInt());
        }

    } else if (command == "GAME_OVER") {
        // 格式：GAME_OVER <BLACK|WHITE|DRAW>
        if (parts.size() >= 2) {
            onGameEnd(parts[1]);
        }

    } else if (command == "WIN") {
        // 兼容旧格式（服务端可能还在使用）
        if (parts.size() >= 2) {
            onGameEnd(parts[1]);
        }

    } else if (command == "STATS_RESULT") {
        // 格式：STATS_RESULT <total> <wins> <losses> <draws>
        if (parts.size() >= 5) {
            onStatsReceived(parts[1].toInt(), parts[2].toInt(),
                            parts[3].toInt(), parts[4].toInt());
        }

    } else if (command == "OPPONENT_DISCONNECTED") {
        onOpponentDisconnected();

    } else if (command == "HEARTBEAT_OK") {
        // 心跳响应，忽略即可
        qDebug() << "[网络] 心跳响应正常";

    } else if (command == "ERROR") {
        // 格式：ERROR <message...>
        QString errorMsg = parts.mid(1).join(" ");
        logTextEdit->append(QString::fromUtf8("[错误] ") + errorMsg);

        // 根据错误类型决定是否弹窗
        if (errorMsg.contains("Login failed") || errorMsg.contains("already exists")
            || errorMsg.contains("Username") || errorMsg.contains("Password")) {
            QMessageBox::warning(this, QString::fromUtf8("操作失败"), errorMsg);
        } else if (errorMsg.contains("Not logged in")) {
            QMessageBox::warning(this, QString::fromUtf8("请先登录"),
                                 QString::fromUtf8("请先登录账号后再进行操作"));
        } else {
            // 其他错误在日志中显示即可，不强制弹窗打断游戏流程
            QMessageBox::warning(this, QString::fromUtf8("服务器错误"), errorMsg);
        }

    } else {
        qDebug() << "[网络] 未知命令:" << command;
    }
}

void MainWindow::onLoginSuccess(const QString &sessionId, int total, int wins, int losses, int draws,
                                const QString &serverUsername)
{
    currentSessionId = sessionId;
    updateStats(total, wins, losses, draws);
    isAIMode = false;
    modeLabel->setText(QString::fromUtf8("模式: 在线"));
    showGamePage();

    // 优先使用服务端返回的用户名，回退到输入框
    QString displayName = serverUsername.isEmpty() ? usernameEdit->text() : serverUsername;
    matchInfoLabel->setText(QString::fromUtf8("欢迎, ") + displayName);
    gameStatusLabel->setText(QString::fromUtf8("已登录，点击「开始匹配」开始游戏"));
    logTextEdit->append(QString::fromUtf8("[登录] 登录成功，欢迎 ") + displayName);
}

void MainWindow::onMatchQueue()
{
    gameStatusLabel->setText(QString::fromUtf8("正在匹配中..."));
}

void MainWindow::onMatchStart(int matchId, const QString &color, const QString &opponentName)
{
    currentMatchId = matchId;
    myColor  = color;
    isPlaying = true;

    gameBoard->reset();

    QString opponent = opponentName.isEmpty() ? QString::fromUtf8("对手") : opponentName;

    if (color == "BLACK") {
        gameBoard->setMyColor(1);  // 1=黑棋
        gameBoard->setEnabled(true);  // 黑方先行，直接启用
        matchInfoLabel->setText(QString::fromUtf8("执黑先行 VS ") + opponent);
        updateTurnStatus(true);  // 轮到我
        logTextEdit->append(QString::fromUtf8("[游戏] 匹配成功！你执黑棋(先行)，对手：") + opponent);
    } else {
        gameBoard->setMyColor(2);  // 2=白棋
        gameBoard->setEnabled(false); // 白方后行，等待对手
        matchInfoLabel->setText(QString::fromUtf8("执白后行 VS ") + opponent);
        updateTurnStatus(false);  // 等待对手
        logTextEdit->append(QString::fromUtf8("[游戏] 匹配成功！你执白棋(后行)，对手：") + opponent);
    }

    surrenderBtn->setEnabled(true);
    matchBtn->setEnabled(false);
}

void MainWindow::onOpponentMove(int x, int y, int moveColor)
{
    // moveColor: 服务端发来的落子方颜色（1=黑, 2=白）
    // 如果服务端未发送颜色（旧格式兼容），则使用对手颜色
    int pieceColor = (moveColor == 1 || moveColor == 2) ? moveColor : gameBoard->getOpponentColor();

    // 安全检查：如果该位置已有棋子，说明状态不同步，记录日志但不覆盖
    if (gameBoard->getPieceColor(x, y) != 0) {
        logTextEdit->append(QString::fromUtf8("[警告] 位置(%1,%2)已有棋子，忽略重复落子").arg(x).arg(y));
        return;
    }

    gameBoard->placePiece(x, y, pieceColor);

    QString colorName = (pieceColor == 1) ? QString::fromUtf8("黑") : QString::fromUtf8("白");
    QString logMsg = QString::fromUtf8("[游戏] %1方落子: (%2,%3)").arg(colorName).arg(x).arg(y);
    logTextEdit->append(logMsg);

    if (isPlaying) {
        updateTurnStatus(true);  // 轮到我了
    }
}

void MainWindow::onYourTurn(int color)
{
    // color: 1=黑方回合, 2=白方回合
    int myColorInt = (myColor == "BLACK") ? 1 : 2;
    bool isMyTurn = (color == myColorInt);

    gameBoard->setEnabled(isMyTurn);
    updateTurnStatus(isMyTurn);

    QString turnColor = (color == 1) ? QString::fromUtf8("黑") : QString::fromUtf8("白");
    QString logMsg = isMyTurn
        ? QString::fromUtf8("[游戏] %1方回合 → 轮到你落子").arg(turnColor)
        : QString::fromUtf8("[游戏] %1方回合 → 等待对手").arg(turnColor);
    logTextEdit->append(logMsg);
}

void MainWindow::updateTurnStatus(bool isMyTurn)
{
    QString myColorName = (myColor == "BLACK") ? QString::fromUtf8("黑") : QString::fromUtf8("白");
    if (isMyTurn) {
        gameStatusLabel->setText(QString::fromUtf8("轮到你落子（%1棋）").arg(myColorName));
    } else {
        QString oppColorName = (myColor == "BLACK") ? QString::fromUtf8("白") : QString::fromUtf8("黑");
        gameStatusLabel->setText(QString::fromUtf8("等待对手落子（%1棋）...").arg(oppColorName));
    }
}

void MainWindow::onGameEnd(const QString &result)
{
    isPlaying = false;
    gameBoard->setEnabled(false);
    surrenderBtn->setEnabled(false);
    matchBtn->setEnabled(true);

    QString resultText;
    QString winnerColor;

    // 适配新格式 BLACK/WHITE/DRAW，同时兼容旧格式"黑方"/"白方"
    if (result == "BLACK" || result == QString::fromUtf8("黑方")) {
        resultText   = QString::fromUtf8("黑方获胜!");
        winnerColor  = (myColor == "BLACK")
                       ? QString::fromUtf8("（你赢了！🎉）")
                       : QString::fromUtf8("（对手赢了）");
    } else if (result == "WHITE" || result == QString::fromUtf8("白方")) {
        resultText   = QString::fromUtf8("白方获胜!");
        winnerColor  = (myColor == "WHITE")
                       ? QString::fromUtf8("（你赢了！🎉）")
                       : QString::fromUtf8("（对手赢了）");
    } else {
        resultText   = QString::fromUtf8("平局!");
        winnerColor  = QString::fromUtf8("");
    }

    gameStatusLabel->setText(QString::fromUtf8("游戏结束: ") + resultText);
    logTextEdit->append(QString::fromUtf8("[游戏] ") + resultText + winnerColor);

    // 刷新战绩
    networkManager->sendStats();

    QMessageBox::information(this, QString::fromUtf8("游戏结束"),
                             resultText + "\n" + winnerColor);
}

void MainWindow::onStatsReceived(int total, int wins, int losses, int draws)
{
    updateStats(total, wins, losses, draws);
}

void MainWindow::onOpponentDisconnected()
{
    isPlaying = false;
    gameBoard->setEnabled(false);
    surrenderBtn->setEnabled(false);
    matchBtn->setEnabled(true);

    gameStatusLabel->setText(QString::fromUtf8("对手断开连接"));
    logTextEdit->append(QString::fromUtf8("[游戏] 对手已断开连接，你获胜了！"));

    QMessageBox::information(this, QString::fromUtf8("对手离开"), QString::fromUtf8("对手已断开连接，你获胜了!"));

    networkManager->sendStats();
}

void MainWindow::onAIMove(int x, int y)
{
    Q_UNUSED(x);
    Q_UNUSED(y);
}