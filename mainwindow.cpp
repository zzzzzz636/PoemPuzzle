//mainwindow.cpp*
#include "mainwindow.h"
#include"solver_util.h"
#include "ui_mainwindow.h"
#include"poemrepository.h"
#include"solverworker.h"
#include"solver_idastar.h"
#include <QMessageBox>
#include <QDebug>
#include<algorithm>
#include<QRandomGenerator>
#include<QTimer>
#include<QInputDialog>
#include <QSettings>
#include<QCoreApplication>
#include<QFileInfo>
#include<QDir>
#include<QDialog>
#include<QTableWidget>
#include<QHeaderView>
#include<QVBoxLayout>
#include<QQueue>

#include<QThread>
#include<QHash>
#include<QCheckBox>
#include<QVBoxLayout>
#include<QDateTime>
#include<QPropertyAnimation>
#include<QAbstractAnimation>
#include<QLabel>
#include<QStatusBar>
#include<QCloseEvent>

using Grid=QVector<QVector<QString>>;
using namespace std;

/**
 * 按“编号排列”生成相邻交换序列:
 *  from[i] = 当前第 i 位置的编号
 *  to[i]   = 目标第 i 位置应放的编号
 *  rows    = true  表示行交换，false 表示列交换
 *
 *  O(N^2)，N≤9 时 <0.01 s
 */
static QVector<Step> makeSwapPlanByIndex(const QVector<int>& from,
                                         const QVector<int>& to,
                                         bool rows)
{
    QVector<int> cur = from;
    int N = cur.size();
    QVector<Step> plan;

    for (int i = 0; i < N; ++i) {
        if (cur[i] == to[i]) continue;

        /* 在后面找到应该移到 i 位的编号 */
        int idx = -1;
        for (int j = i + 1; j < N; ++j)
            if (cur[j] == to[i]) { idx = j; break; }

        /* 冒泡把 idx 挪到 i */
        for (int k = idx; k > i; --k) {
            std::swap(cur[k], cur[k - 1]);
            plan.push_back({ rows, k - 1, k });
        }
    }
    return plan;       // 最多 N(N-1)/2 步，9×9 ≤36
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),updateTimer(nullptr)
{
    ui->setupUi(this);
    toolLayout = qobject_cast<QVBoxLayout*>(ui->rightPanel->layout());
    if (!toolLayout) {
        toolLayout = new QVBoxLayout(ui->rightPanel);
        ui->rightPanel->setLayout(toolLayout);
    }


    updateTimer = new QTimer(this);
    updateTimer->setInterval(1000);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateTimeLabel);
    connect(updateTimer,&QTimer::timeout,this,[this]{addPlaySec(1);
        static int tick = 0;
        if (++tick >= 15) {             // 每 15 秒自动存
            autoQuickSave();
            tick = 0;
        }
});
    initUI();
    buildPoemMap();
    /* --- 半透明遮罩，用来暂停 --- */
    pauseCover = new QWidget(this);             // 确保头文件里声明 QWidget* pauseCover;
    pauseCover->setStyleSheet("background:rgba(0,0,0,120);");
    pauseCover->hide();
    pauseCover->setGeometry(centralWidget()->geometry()); // 覆盖棋盘
    pauseCover->raise();                                   // 保证在最上层

    saveFilePath=QCoreApplication::applicationDirPath()+"/save.ini";
    QString inputName=QInputDialog::getText(this,"欢迎来到诗词拼图","请输入你的昵称：");
    if(inputName.isEmpty())
    {userName="游客";}
    else{userName=inputName;}
    ui->playerLabel->setText(tr("玩家：%1").arg(userName));
    // —— 检测 autosave / save ——
    const QString dir = QCoreApplication::applicationDirPath();
    const QString autoFile   = dir + "/autosave.ini";
    const QString normalFile = dir + "/save.ini";
    QString file2Load;

    if (QFile::exists(autoFile))       file2Load = autoFile;
    else if (QFile::exists(normalFile)) file2Load = normalFile;

    if (!file2Load.isEmpty()) {
        if (QMessageBox::question(this, tr("恢复存档"),
                                  tr("检测到上次存档，是否恢复？"))
            == QMessageBox::Yes)
        {
            loadGame(file2Load);                 // 重载版
            if (file2Load == autoFile) QFile::remove(autoFile);
        }
    }
    connect(ui->poemSelector,&QComboBox::currentTextChanged,this,&MainWindow::loadPoem);
    ui->poemSelector->setCurrentIndex(0);
    loadPoem(ui->poemSelector->currentText());
    _cfg.beginGroup("Stat");
    _stat.playSec    = _cfg.value("play",0).toInt();
    _stat.cleared    = _cfg.value("clr",0).toInt();

    _stat.min3       = _cfg.value("min3",9999).toInt();
    _stat.min4       = _cfg.value("min4",9999).toInt();
    _stat.min5       = _cfg.value("min5",9999).toInt();
    _stat.min6       = _cfg.value("min6",9999).toInt();
    _cfg.endGroup();

    _cfg.beginGroup("Ach");
    for (auto k : _cfg.childKeys()) _ach[k] = _cfg.value(k).toBool();
    _cfg.endGroup();
    connect(ui->btnStat, &QPushButton::clicked, this, [this]{
        QString info = tr("累计在线 %1 分 %2 秒\n"
                          "通关 %3 首\n最快 %4 秒\n"
                          "3×3最少 %5 步\n4×4最少 %6 步\n5×5最少 %7 步")
                           .arg(_stat.playSec/60)
                           .arg(_stat.playSec%60, 2, 10, QChar('0'))
                           .arg(_stat.cleared)
                           .arg(_stat.fastestSec == 9999 ? 0 : _stat.fastestSec)
                           .arg(_stat.min3 == 9999 ? 0 : _stat.min3)
                           .arg(_stat.min4 == 9999 ? 0 : _stat.min4)
                           .arg(_stat.min5 == 9999 ? 0 : _stat.min5)
                           .arg(_stat.min6 == 9999 ? 0 : _stat.min6);
           QStringList unlocked;
           for (auto it=_ach.cbegin(); it!=_ach.cend(); ++it)
               if (it.value()) unlocked << achName.value(it.key());
           info += tr("\n\n已解锁成就：%1").arg(unlocked.join("、"));
        QMessageBox::information(this, tr("个人成就"), info);
    });


    auto *night = new QCheckBox("夜间模式", this);
                  toolLayout->addWidget(night);
    connect(night, &QCheckBox::toggled, this,  &MainWindow::togglenight);
                  resetClock();

}


MainWindow::~MainWindow() {
    delete ui;
}
void MainWindow::togglenight(bool on)
{
    static const char *dark =
        "QWidget{background:#2b2b2b;color:#dcdcdc}"
                              "QPushButton{border:4px solid #555;border-radius:4px}";


    static const char *light =
        "QWidget{background:#fafafa;color:#000}";

    qApp->setStyleSheet(on ? dark : light);
}
void MainWindow::resetClock()
{
    updateTimer->stop();           // 停 UI 刷新
    timeCounter.invalidate();      // 秒表失效
    gameRunning = false;
    elapsedAcc=0;

    ui->timeLabel->setText("用时 00:00");
}
void MainWindow::initUI() {

    connect(ui->restartButton, &QPushButton::clicked, this, &MainWindow::restartCurrentPoem);

    connect(ui->sizeSelector, &QComboBox::currentTextChanged, this,&MainWindow::changeGridSize);
    connect(ui->instructionButton,&QPushButton::clicked,this,&MainWindow::showInstruction);
    connect(ui->saveButton,&QPushButton::clicked,this,[=]()
            {saveGame(true);});

    connect(ui->saveButton, &QPushButton::clicked,
            this, [this](bool){ saveGame(true); });

    connect(ui->clearButton,&QPushButton::clicked,this,&MainWindow::clearGame);
    connect(ui->rankingButton,&QPushButton::clicked,this,&MainWindow::showRanking);
    connect(ui->globalRankingButton,&QPushButton::clicked,this,&MainWindow::showGlobalRanking);
    connect(ui->autoButton,&QPushButton::clicked,this,&MainWindow::autoSolve);

    ui->moveLabel->setText(QString("步数：%1").arg(moveCount));
    ui->playerLabel->setText(QString("玩家：%1").arg(userName));
    initGridUI(); // 初始化按钮
    ui->sizeSelector->clear();
    for(int s:{3,4,5,6})
        ui->sizeSelector->addItem(QString("%1X%1").arg(s));


    ui->timeLabel->setText("用时00:00");
    // mainwindow.cpp  构造函数末尾
    connect(ui->btnUndo , &QToolButton::clicked, this, &MainWindow::undo);

    connect(ui->btnPause, &QToolButton::clicked, this, &MainWindow::togglePause);
    connect(ui->btnHint , &QToolButton::clicked, this, &MainWindow::showHint);

    // 保留快捷键：把 actions 加到窗口
    ui->btnUndo ->setShortcut(QKeySequence::Undo);
    ui->btnRedo ->setShortcut(QKeySequence::Redo);
    ui->btnPause->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_P));
    ui->btnHint ->setShortcut(Qt::Key_H);
    QPushButton *challengeButton = new QPushButton(tr("开始挑战赛"), this);
    connect(challengeButton, &QPushButton::clicked,
            this, &MainWindow::startChallengeMode);
    toolLayout->addWidget(challengeButton);

    QPushButton *challengeRankingButton = new QPushButton(tr("挑战赛排行榜"), this);
    connect(challengeRankingButton, &QPushButton::clicked,
            this, &MainWindow::showChallengeRanking);
    toolLayout->addWidget(challengeRankingButton);
    sideButtons.append(challengeButton);
    sideButtons.append(challengeRankingButton);
    const int kBtnW = 120;                    // 想调就改这一行
    for (auto *b : sideButtons) {
        b->setFixedWidth(kBtnW);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    }

    // —— 新增：在状态栏添加 QLabel，用于显示倒计时和得分 ——
    challengeInfo = new QLabel(this);
    challengeInfo->setStyleSheet("font-size:16px; font-weight:bold; color:red;");
    challengeInfo->setAlignment(Qt::AlignCenter);
    challengeInfo->hide();
    statusBar()->addWidget(challengeInfo);
    exitChallengeBtn=new QPushButton(tr("退出挑战赛"),this);
    exitChallengeBtn->setEnabled(false);                     // 默认灰
    toolLayout->addWidget(exitChallengeBtn);
    sideButtons.append(exitChallengeBtn);

 connect(exitChallengeBtn, &QPushButton::clicked,this, [this]{
                              stopChallengeMode(false);
                               exitChallengeBtn->setEnabled(false);
                            });


    auto *origBtn = new QPushButton(tr("查看原诗"), this);
    toolLayout->addWidget(origBtn);
    connect(origBtn, &QPushButton::clicked, this, [this]() {
        // 拿到当前下拉框选中的标题
        QString title = ui->poemSelector->currentText();
        // 从题库里取出对应 Poem
        Poem p = PoemRepository::instance().poemByTitle(title);
        // 拼接成多行字符串
        QString text = p.lines.join("\n");
        // 弹窗显示
        QMessageBox::information(this,
                                 tr("原诗：%1").arg(title),
                                 text);
    });

}
void MainWindow::initGridUI()
{

    while (QLayoutItem *item = ui->gridLayout->takeAt(0)) {
        if (auto w = item->widget()) delete w;
        delete item;
    }
    buttons.clear();


    buttons.resize(gridSize);
    for (int i = 0; i < gridSize; ++i) {
        buttons[i].resize(gridSize);
        for (int j = 0; j < gridSize; ++j) {
            auto *btn = new QPushButton(this);
            btn->setFixedSize(60, 60);
            connect(btn, &QPushButton::clicked,
                    this, &MainWindow::handleButtonClicked);
            ui->gridLayout->addWidget(btn, i, j);
            buttons[i][j] = btn;
        }
    }
    resetClock();
}
void MainWindow::buildPoemMap() {
    // 读取 JSON
    QString jsonPath =  ":/data/poems.json";
    if(!PoemRepository::instance().load(jsonPath)) {
        QMessageBox::critical(this,"题库读取失败",
                              "无法读取 poems.json，程序将退出"); exit(1);
    }

    // 填充下拉框
    for(const Poem& p : PoemRepository::instance().poems())
        ui->poemSelector->addItem(p.title);
}
void MainWindow::loadPoem(const QString& title) {
    Poem p = PoemRepository::instance().poemByTitle(title);
    if (p.lines.isEmpty()) return;

    if (p.size != gridSize) {
        gridSize = p.size;
        initGridUI();
    }
    // -------- 把右侧尺寸框跳到对应尺寸 ----------
    const QString want = QString("%1X%1").arg(gridSize);
    ui->sizeSelector->blockSignals(true);          // 防止递归触发 changeGridSize()
    ui->sizeSelector->setCurrentText(want);
    ui->sizeSelector->blockSignals(false);
    current.clear();
    current.resize(gridSize);
    for (int i = 0; i < gridSize; ++i) {
        QString line = p.lines[i]; // 取第 i 行原句
        QStringList chars;
        for (QChar ch : line) chars << QString(ch);

        // 补足或截断字符
        if (chars.size() < gridSize) {
            while (chars.size() < gridSize) chars << " ";
        } else if (chars.size() > gridSize) {
            chars = chars.mid(0, gridSize);
        }
        current[i] = chars;
    }

    goal = current; // 目标局面


    int shuffleTimes = gridSize * gridSize * 100;      // 洗牌步数

    std::mt19937 rng{ std::random_device{}() };      // 随机引擎
    std::uniform_int_distribution<int> coin(0,1);    // 行 / 列 二选一
    std::uniform_int_distribution<int> pos(0, gridSize - 2); // 相邻下标

    for (int t = 0; t < shuffleTimes; ++t) {
        int idx = pos(rng);
        if (coin(rng) == 0) {                        // 行交换
            std::swap(current[idx], current[idx + 1]);
        } else {                                     // 列交换
            for (int r = 0; r < gridSize; ++r)
                std::swap(current[r][idx], current[r][idx + 1]);
        }
    }
    initGridUI(); // 重画按钮
    updateGrid(); // 更新界面
    firstClick = {-1, -1}; // 清空选中状态
    moveCount  = 0;
    ui->moveLabel->setText("步数：0");
    updateTimer->stop();          // ★清零计时
    ui->timeLabel->setText("用时00:00");
    resetClock();
}



void MainWindow::updateGrid()
{
    for (int i = 0; i < gridSize; ++i)
        for (int j = 0; j < gridSize; ++j)
            buttons[i][j]->setText(current[i][j]);
}


void MainWindow::handleButtonClicked()
{
    // 1. 找到被点按钮在棋盘中的行列 ---------------------------
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;                          // 理论上不会空

    int r = -1, c = -1;
    for (int i = 0; i < gridSize && r == -1; ++i)
        for (int j = 0; j < gridSize; ++j)
            if (buttons[i][j] == btn) { r = i; c = j; break; }

    // 2. 若是第一次点击，记录并高亮，直接返回 -------------------
    if (firstClick.first == -1) {
        firstClick = {r, c};
        highlightButton(r, c, /*on=*/true);    // 可选：给按钮上边框
        return;
    }

    // 3. 第二次点击 —— 先撤掉上一个高亮 ------------------------
    highlightButton(firstClick.first, firstClick.second, /*on=*/false);

    // 4. 判断是否相邻
    int dr = qAbs(r - firstClick.first);
    int dc = qAbs(c - firstClick.second);
    bool legal = (dr + dc == 1);               // 只允许上下左右相邻

    if (legal) {
        Step step;                            // ① 组装一步
        if (dr == 1) {                        // 行交换
            step.isRow = true;
            step.a     = r;
            step.b     = firstClick.first;
        } else {                              // 列交换
            step.isRow = false;
            step.a     = c;
            step.b     = firstClick.second;
        }

        applyStep(step);                         // 计步、计时、存档、检测胜利
    }
    // 5. 重置状态，等待下一轮点击 ------------------------------
    firstClick = {-1, -1};
}

//---------------------------------------------------------------
// 小工具 1：高亮 / 取消高亮棋盘按钮
//---------------------------------------------------------------
void MainWindow::highlightButton(int row, int col, bool on)
{
    static const QString hlStyle = "border:2px solid #E86;";
    if (on){
        buttons[row][col]->setStyleSheet(hlStyle);
    buttons[row][col]->setFont(QFont("",11,QFont::Bold));
    }
    else
    {buttons[row][col]->setStyleSheet({});
        buttons[row][col]->setFont(QFont("",11,QFont::Normal));}
}


void MainWindow::swapRows(int r1, int r2)
{
    for (int j = 0; j < gridSize; ++j) {
        std::swap(current[r1][j], current[r2][j]);           // 数据
        buttons[r1][j]->setText(current[r1][j]);             // UI
        buttons[r2][j]->setText(current[r2][j]);             // UI
    }
}

void MainWindow::swapCols(int c1, int c2)
{
    for (int i = 0; i < gridSize; ++i) {
        std::swap(current[i][c1], current[i][c2]);           // 数据
        buttons[i][c1]->setText(current[i][c1]);             // UI
        buttons[i][c2]->setText(current[i][c2]);             // UI
    }
}
bool MainWindow::isSolved()
{for (int i=0;i<gridSize;++i)
    {for(int j=0;j<gridSize;++j)
        {if(current[i][j]!=goal[i][j])
                return false;
        }
    }
    return true;
}
void MainWindow::restartCurrentPoem()
{QString title=ui->poemSelector->currentText();
    loadPoem(title);
    moveCount=0;
    ui->moveLabel->setText(QString("步数：%1").arg(moveCount));
resetClock();
}

void MainWindow::changeGridSize(const QString& text)
{
    int newSize = text.left(text.indexOf('X')).toInt();
    if (newSize == gridSize) return;          // 同尺寸，直接返回
    ui->poemSelector->blockSignals(true);
    ui->poemSelector->clear();
    for (const Poem& p : PoemRepository::instance().poems())
        if (p.size == newSize)
            ui->poemSelector->addItem(p.title);
    ui->poemSelector->blockSignals(false);
    gridSize = newSize;
    firstClick={-1,-1};
    initGridUI();        // 按新尺寸重建格子

    loadPoem(ui->poemSelector->currentText());   // 重新加载当前诗句

    moveCount = 0;
    ui->moveLabel->setText("步数：0");
    updateTimer->stop();
    ui->timeLabel->setText("用时00:00");
    resetClock();
}
void MainWindow::showInstruction()
{QString message=
        "【游戏规则】\n"
        "1.点击任意一个按钮，再点击它的相邻（上下左右）的另一个按钮，即可交换这两行或两列的位置。\n"
        "2.通关目标：将打乱的诗句恢复为正常顺序。\n"
        "3.游戏支持3x3,4x4,5x5,6x6四种不同难度。\n"
        "4.自动求解按钮可帮助你完成游戏，但将不保存为您的成绩且游戏结束。\n"
        "5.挑战模式一次成功完成五首诗词即挑战成功。\n"
        "祝你游戏愉快！";
    QMessageBox::information(this,"游戏说明",message);}
void MainWindow::saveGame(bool showMsg /*=true*/, bool writeRanking /*=true*/)
{
    QFileInfo fi(saveFilePath);
    QDir dir = fi.dir();
    if(!dir.exists()) dir.mkpath(dir.absolutePath());

    /* -------- 先建 QSettings -------- */
    QSettings st(saveFilePath, QSettings::IniFormat);
    st.beginGroup(userName);

    /* -------- 普通存档 -------- */
    QString title = ui->poemSelector->currentText();
    st.setValue("poem",      title);
    st.setValue("gridSize",  gridSize);
    st.setValue("moveCount", moveCount);

    QStringList flat;
    for (int i = 0; i < gridSize; ++i)
        for (int j = 0; j < gridSize; ++j)
            flat << current[i][j];
    st.setValue("current", flat);

    /* -------- 计算总用时（在清零之前！） -------- */
    qint64 totalMs = elapsedAcc;                         // 已累计
    if (timeCounter.isValid())                           // 还在跑或刚停
        totalMs += timeCounter.elapsed();

    /* -------- 写排行榜 -------- */
    if (writeRanking && current == goal) {               // 只在通关时
        st.beginGroup("records");
        QString key = QString("%1_%2").arg(title).arg(gridSize);
        QString newTime = QTime(0,0).addMSecs(totalMs).toString("mm:ss");
        QString newRec  = QString("%1,%2").arg(newTime).arg(moveCount);

        /* 若已有旧纪录，只保留更快/步数更少者 */
        QString old = st.value(key).toString();          // 可能为空
        bool better = old.isEmpty();
        if (!better) {
            auto p = old.split(',');
            better = (newTime < p[0]) ||
                     (newTime == p[0] && moveCount < p[1].toInt());
        }
        if (better) st.setValue(key, newRec);
        st.endGroup();                                   // /records
    }

    st.endGroup();                                       // /<user>

    /* -------- 现在才清零计时器 -------- */
    resetClock();

    if (showMsg)
        QMessageBox::information(this,"存档","存档成功！");
}

void MainWindow::loadGame(const QString& filePath)
{
    const QString bak = saveFilePath;    // 记住原路径
    saveFilePath = filePath;
    loadGame();                          // 复用老逻辑
    saveFilePath = bak;                  // 还原
}
void MainWindow::loadGame() {
    QFileInfo fi(saveFilePath);
    if (!fi.exists()) {
        QMessageBox::warning(this, "读取存档", "没有找到存档文件！");
        return;
    }

    QSettings settings(saveFilePath, QSettings::IniFormat);
    if (!settings.childGroups().contains(userName)) {
        QMessageBox::warning(this, "读取存档", "当前用户没有存档！");
        return;
    }

    settings.beginGroup(userName);
    QString title = settings.value("poem").toString();
    int sz = settings.value("gridSize").toInt();
    int cnt = settings.value("moveCount").toInt();
    QStringList flat = settings.value("current").toStringList();
    settings.endGroup();

    if (flat.size() != sz*sz) {
        QMessageBox::warning(this, "读取存档", "存档数据与当前尺寸不符！");
        return;
    }

    // 切换尺寸、重建
    gridSize = sz;
    initGridUI();

    // 填充 current
    current.resize(gridSize);
    for (int i = 0, k = 0; i < gridSize; ++i) {
        current[i].resize(gridSize);
        for (int j = 0; j < gridSize; ++j)
            current[i][j] = flat[k++];
    }

    // 切换诗句下拉框
    ui->poemSelector->setCurrentText(title);

    // 更新界面和标签
    updateGrid();
    moveCount = cnt;
    ui->playerLabel->setText(QString("玩家：%1").arg(userName));
    ui->moveLabel->setText(QString("步数：%1").arg(moveCount));

    QMessageBox::information(this, "读取存档", "存档加载完成！");
    goal=current;
    resetClock();
}
void MainWindow::clearGame() {
    if (QFile::remove(saveFilePath)) {
        QMessageBox::information(this, "清除存档", "存档已删除");
    } else {
        QMessageBox::warning(this, "清除存档", "没有找到存档文件");
    }
    restartCurrentPoem();
    moveCount = 0;
    ui->moveLabel->setText("步数：0");
}
void MainWindow::updateTimeLabel()
{
    qint64 totalMs = elapsedAcc +
                     (gameRunning ? timeCounter.elapsed() : 0);

    QTime t0(0,0);   // 00:00
    ui->timeLabel->setText("用时 " +
                           t0.addMSecs(totalMs).toString("mm:ss"));
}
void MainWindow::showRanking() {

    QDialog dlg(this);
    dlg.setWindowTitle(QString("%1的排行榜").arg(userName));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);


    QTableWidget *table = new QTableWidget(&dlg);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"关卡", "用时", "步数"});


    QSettings st(saveFilePath, QSettings::IniFormat);
    st.beginGroup(userName + "/records");
    QStringList keys = st.childKeys();
    table->setRowCount(keys.size());

    int row = 0;
    for (const QString &key : keys) {
        QStringList keyParts = key.split('_');
        QString poem = keyParts[0];
        int     sz   = keyParts[1].toInt();
        QString rec  = st.value(key).toString();
        auto recParts = rec.split(',');

        table->setItem(row, 0, new QTableWidgetItem(
                                   QString("%1 (%2×%2)").arg(poem).arg(sz)
                                   ));
        table->setItem(row, 1, new QTableWidgetItem(recParts[0]));
        table->setItem(row, 2, new QTableWidgetItem(recParts[1]));
        row++;
    }
    st.endGroup();

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    lay->addWidget(table);


    dlg.exec();
}
void MainWindow::showGlobalRanking()
{

    struct Record { QString user, poem; int size; QString time; int steps; };
    QVector<Record> all;

    QSettings st(saveFilePath, QSettings::IniFormat);
    QStringList users = st.childGroups();
    for (const QString &u : users) {
        st.beginGroup(u);
        st.beginGroup("records");
        for (const QString &key : st.childKeys()) {
            QStringList k = key.split('_');
            QString poem  = k[0];
            int sz        = k[1].toInt();
            QStringList v = st.value(key).toString().split(',');
            all.append({u, poem, sz, v[0], v[1].toInt()});
        }
        st.endGroup();
        st.endGroup();
    }


    std::sort(all.begin(), all.end(), [](const Record &a, const Record &b){
        if (a.time == b.time) return a.steps < b.steps;
        return a.time < b.time;
    });


    QDialog dlg(this);
    dlg.setWindowTitle("全服排行榜");
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QTableWidget *table = new QTableWidget(&dlg);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"玩家", "关卡", "用时", "步数", "尺寸"});
    table->setRowCount(all.size());

    int r = 0;
    for (const auto &rec : all) {
        table->setItem(r,0,new QTableWidgetItem(rec.user));
        table->setItem(r,1,new QTableWidgetItem(rec.poem));
        table->setItem(r,2,new QTableWidgetItem(rec.time));
        table->setItem(r,3,new QTableWidgetItem(QString::number(rec.steps)));
        table->setItem(r,4,new QTableWidgetItem(QString("%1×%1").arg(rec.size)));
        r++;
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    lay->addWidget(table);
    dlg.exec();
}


void MainWindow::applyStep(const Step &s, bool recordUndo)
{
    if(s.isRow)swapRows(s.a,s.b);
 else swapCols(s.a,s.b);
    if (recordUndo) { undoStack.push(s); redoStack.clear(); }
    ++moveCount;
    ui->moveLabel->setText(QString("步数：%1").arg(moveCount));
    if(++autoCnt>=kAutoTHreshold)
    {autoQuickSave();
    autoCnt=0;}
    /* ------ ⏱️ 首次启动计时 ------ */
    if (!timeCounter.isValid()) {               // 还没启动过
        timeCounter.start();
        updateTimer->start(1000);               // 1 秒刷新
        gameRunning = true;
    }

    /* ------ 通关检测 ------ */
    if (isSolved()) {
        if(!byAuto){
        elapsedAcc += timeCounter.elapsed();    // 结算最终用时
        updateTimer->stop();
        levelFinishedHook(gridSize, moveCount, elapsedAcc + timeCounter.elapsed());

        gameRunning = false;

        saveGame(false);                        // 写排行榜

        QMessageBox::information(this,"恭喜","已完成！");

        moveCount = 0;
        ui->moveLabel->setText("步数：0");

        undoStack.clear();
        redoStack.clear();
        timeCounter.invalidate();
        elapsedAcc = 0;}
        else{byAuto=false;

        return;}


    }
    if (challengeMode && isSolved()) {
        levelFinishedHook(gridSize, moveCount, elapsedAcc + timeCounter.elapsed());

        int timeBonus = challengeRemainingTime * 10;
        int stepPenalty = moveCount * 2;
        int poemScore = 100 + timeBonus - stepPenalty;
        if (poemScore < 50) poemScore = 50;
        challengeScore += poemScore;
        QTimer::singleShot(1500, this, &MainWindow::nextChallengePoem);
    }
}
void MainWindow::onSolverStep(const Step& step) {
    applyStep(step);               // 应用每一步
    updateGrid();                  // 更新界面
    QCoreApplication::processEvents(); // 确保界面刷新
}
void MainWindow::autoQuickSave()
{
    saveGame(/*showMsg*/false, /*writeRanking*/false);
    qDebug() << "Auto-saved";            // 日志方便调试
}


void MainWindow::closeEvent(QCloseEvent *ev)
{
    /* —— 写入 autosave.ini —— */
    QString path = QCoreApplication::applicationDirPath() + "/autosave.ini";
    saveFilePath = path;          // 临时改写一下目标文件
    saveGame(false, false);       // 静默保存（不弹窗、不写排行榜）
    saveFilePath = QCoreApplication::applicationDirPath() + "/save.ini";

    QMainWindow::closeEvent(ev);
}
QVector<Step> MainWindow::solveRowCol(const Grid& cur,const Grid& goal, int N)


{
    // -------- 行映射 --------
    QHash<QString, QList<int>> rowBuckets;
    for (int g = 0; g < N; ++g)
        rowBuckets[ goal[g].join("") ].append(g);

    QVector<int> rowFrom, rowTo;
    for (int i = 0; i < N; ++i) {
        rowFrom << i;
        rowTo   << rowBuckets[ cur[i].join("") ].takeFirst();
    }
    QVector<Step> rowSteps = makeSwapPlanByIndex(rowFrom, rowTo, true);

    // 把行步骤作用到 mid
    Grid mid = cur;
    for (const Step& st : rowSteps)
        std::swap(mid[st.a], mid[st.b]);

    // -------- 列映射 --------
    auto colStr=[&](const Grid& G,int c){
        QString s; s.reserve(N);
        for(int r=0;r<N;++r) s+=G[r][c];
        return s;
    };
    QHash<QString, QList<int>> colBuckets;
    for (int g=0; g<N; ++g)
        colBuckets[ colStr(goal,g) ].append(g);

    QVector<int> colFrom, colTo;
    for (int c=0;c<N;++c){
        colFrom<<c;
        colTo  << colBuckets[ colStr(mid,c) ].takeFirst();
    }
    QVector<Step> colSteps = makeSwapPlanByIndex(colFrom, colTo, false);

    return rowSteps + colSteps;   // 合并
}

void MainWindow::autoSolve()
{
    if (autoSolving) return;
    byAuto=true;
    autoSolving = true;
    ui->autoButton->setEnabled(false);

    // 保存当前游戏状态
    bool wasGameRunning = gameRunning;
    qint64 elapsedTime = timeCounter.isValid() ? timeCounter.elapsed() : 0;

    updateTimer->stop();
    gameRunning = false;

    QVector<Step> steps;
    const int N = gridSize;

    try {
        if (N <= 6)
            steps = bfsSolve(current, goal, N);
        else
            steps = idaStarSolve(current, goal, N, 30000);

        if (steps.isEmpty() && current != goal) {
            QMessageBox::warning(this, "自动求解", "30秒内未找到解");
        } else {
            for (const Step& st : steps) {
                applyStep(st);
                QCoreApplication::processEvents();
                QThread::msleep(100);
            }

            // 自动求解完成后询问是否重新开始
            auto reply = QMessageBox::question(
                this,
                "自动求解完成",
                "自动求解已完成，是否重新开始游戏？",
                QMessageBox::Yes | QMessageBox::No
                );

            if (reply == QMessageBox::Yes) {
                restartCurrent();
            }
        }
    } catch (...) {
        QMessageBox::warning(this, "错误", "求解过程中发生异常");
    }

    // 恢复游戏状态（如果用户选择不重新开始）
    if (wasGameRunning && !gameRunning) {
        elapsedAcc =elapsedTime;
        timeCounter.start();
        updateTimer->start();
        gameRunning = true;
    }

    ui->autoButton->setEnabled(true);
    autoSolving = false;
    saveGame(false,false);
}
void MainWindow::restartCurrent()
{
    QString title = ui->poemSelector->currentText();
    loadPoem(title); // 这会重新加载并打乱诗词

    // 重置游戏状态
    moveCount = 0;
    ui->moveLabel->setText("步数：0");
    firstClick = {-1, -1}; // 清空选中状态

    // 重置计时器
    resetClock();

    // 清除自动求解状态
    if (playTimer && playTimer->isActive()) {
        playTimer->stop();
    }
    pendingSteps.clear();

    // 重置按钮状态
    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            buttons[i][j]->setStyleSheet("");
        }
    }
}
void MainWindow::playNextStep()
{
    if (pendingSteps.isEmpty()) {
        /* 解完 */
        autoSolving = false;
        saveGame(false);
        return;
    }
    applyStep(pendingSteps.takeFirst());
}
// ---------- 撤销 ----------
void MainWindow::undo()
{
    if (undoStack.isEmpty() || !pauseCover->isHidden())
        return;                               // 栈空 or 暂停中

    const Step last = undoStack.pop();        // 取最后一步
    swapLinesOrCols(this,last.reversed());
    redoStack.push(last);                     // 放进重做栈

    --moveCount;                              // ★ 手动减 1
    ui->moveLabel->setText(QString("步数：%1").arg(moveCount));
}


void MainWindow::togglePause()
{

    if (pauseCover->isHidden()) {           // → 暂停
        elapsedAcc += timeCounter.elapsed();
        timeCounter.invalidate();
        updateTimer->stop();
        gameRunning = false;
        pauseCover->show();
    } else {                                // → 继续
        timeCounter.start();
        updateTimer->start();
        gameRunning = true;
        pauseCover->hide();
    }
}
QVector<Step> MainWindow::nextHint()
{QVector<Step> path=bfsSolve(current,goal,gridSize);
    if (!path.isEmpty() || current == goal) return path;

auto quick = solveRowCol(current, goal, gridSize);
  return quick;}
void MainWindow::showHint()
{
    if (!pauseCover->isHidden()) return;

    QVector<Step> path =nextHint();    // 不限层
    if (path.isEmpty()) {                            // ★ 防空
        QMessageBox::information(this, "提示", "已是终局或无解。");
        return;
    }
    highlightStep(path.first());
}

void MainWindow::highlightStep(const Step &s)
{
    if (s.a < 0 || s.b < 0 || s.a >= gridSize || s.b >= gridSize)
        return;                                      // ★ 非法一步

    QVector<QPushButton*> targets;
    if (s.isRow) {
        for (int j = 0; j < gridSize; ++j)
            targets << buttons[s.a][j] << buttons[s.b][j];
    } else {
        for (int i = 0; i < gridSize; ++i)
            targets << buttons[i][s.a] << buttons[i][s.b];
    }

    for (auto *btn : targets) btn->setStyleSheet("background:#ffff80;");

           QTimer::singleShot(400, this, [targets](){          // ★ 0.4 s 后恢复
                   for (auto *b : targets) b->setStyleSheet("");
              });

}
QString MainWindow::formatTime(int seconds)
{
    int minutes = seconds / 60;
    int secs = seconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs,    2, 10, QLatin1Char('0'));
}

void MainWindow::loadChallengePoems()
{
    challengePoems.clear();
    challengePoems << Poem{"静夜思", 4, {"床前明月光", "疑是地上霜", "举头望明月", "低头思故乡"}};
    challengePoems << Poem{"登鹳雀楼", 4, {"白日依山尽", "黄河入海流", "欲穷千里目", "更上一层楼"}};
    challengePoems << Poem{"春晓", 4, {"春眠不觉晓", "处处闻啼鸟", "夜来风雨声", "花落知多少"}};
    challengePoems << Poem{"相思", 4, {"红豆生南国", "春来发几枝", "愿君多采撷", "此物最相思"}};
    challengePoems << Poem{"江雪", 4, {"千山鸟飞绝", "万径人踪灭", "孤舟蓑笠翁", "独钓寒江雪"}};


}

void MainWindow::startChallengeMode()
{
    if (challengeMode) return;

    challengeMode = true;
    challengeScore = 0;
    challengeRemainingTime = challengeTimeLimit;

    loadChallengePoems();
    if (challengePoems.isEmpty()) {
        QMessageBox::warning(this, tr("挑战赛"),
                             tr("没有可用的挑战赛诗词！"));
        challengeMode = false;
        return;
    }

    if (!challengeTimer) {
        challengeTimer = new QTimer(this);
        connect(challengeTimer, &QTimer::timeout,
                this, &MainWindow::updateChallengeTimer);
    }

    ui->poemSelector->setEnabled(false);
    ui->sizeSelector->setEnabled(false);
    ui->autoButton->setEnabled(false);
    ui->saveButton->setEnabled(false);
    ui->loadButton->setEnabled(false);

    challengeInfo->setText(
        tr("挑战赛模式 - 剩余时间: %1  得分: %2")
            .arg(formatTime(challengeRemainingTime))
            .arg(challengeScore)
        );
    challengeInfo->show();

    nextChallengePoem();
    challengeTimer->start(1000);
    exitChallengeBtn->setEnabled(true);
}

void MainWindow::stopChallengeMode(bool success)
{
    if (!challengeMode) return;

    if (challengeTimer && challengeTimer->isActive()) {
        challengeTimer->stop();
    }
    if (success) {
        saveChallengeResult();
        challengeWinHook();
    }

    ui->poemSelector->setEnabled(true);
    ui->sizeSelector->setEnabled(true);
    ui->autoButton->setEnabled(true);
    ui->saveButton->setEnabled(true);
    ui->loadButton->setEnabled(true);

    challengeInfo->hide();

    if (success) {
        QMessageBox::information(this, tr("挑战赛完成"),
                                 tr("恭喜完成挑战赛！\n得分: %1\n剩余时间: %2")
                                     .arg(challengeScore)
                                     .arg(formatTime(challengeRemainingTime)));
    } else {
        QMessageBox::information(this, tr("挑战赛失败"),
                                 tr("时间到！挑战赛失败。"));
    }

    challengeMode = false;
    exitChallengeBtn->setEnabled(false);
}

void MainWindow::updateChallengeTimer()
{
    if (--challengeRemainingTime < 0) challengeRemainingTime = 0;
    challengeInfo->setText(
        tr("挑战赛模式 - 剩余时间: %1  得分: %2")
            .arg(formatTime(challengeRemainingTime))
            .arg(challengeScore)
        );
    if (challengeRemainingTime == 0) {
        stopChallengeMode(false);
    }
}

void MainWindow::nextChallengePoem()
{
    if (challengePoems.isEmpty()) {
        stopChallengeMode(true);
        return;
    }
    int idx = QRandomGenerator::global()->bounded(challengePoems.size());
    Poem selectedPoem = challengePoems.takeAt(idx);

    gridSize = selectedPoem.size;
    initGridUI();

    current.clear();
    current.resize(gridSize);
    for (int i = 0; i < gridSize; ++i) {
        QString line = selectedPoem.lines[i];
        QStringList chars;
        for (QChar ch : line) chars << QString(ch);
        if (chars.size() < gridSize) {
            while (chars.size() < gridSize) chars << " ";
        } else if (chars.size() > gridSize) {
            chars = chars.mid(0, gridSize);
        }
        current[i] = chars;
    }
    goal = current;

    int shuffleTimes = gridSize * gridSize * 5;
    for (int t = 0; t < shuffleTimes; ++t) {
        int r = QRandomGenerator::global()->bounded(gridSize - 1);
        bool swapRow = (QRandomGenerator::global()->bounded(2) == 0);
        if (swapRow) {
            swapRows(r, r + 1);
        } else {
            swapCols(r, r + 1);
        }
    }

    updateGrid();
    firstClick = {-1, -1};
    moveCount = 0;
    ui->moveLabel->setText("步数：0");
    ui->poemSelector->blockSignals(true);
    int combIndex = ui->poemSelector->findText(selectedPoem.title);
    if (combIndex >= 0) {
        ui->poemSelector->setCurrentIndex(combIndex);
    }
    ui->poemSelector->blockSignals(false);
}

void MainWindow::saveChallengeResult()
{
    QSettings settings(saveFilePath, QSettings::IniFormat);
    settings.beginGroup("ChallengeRanking");

    int arrSize = settings.beginReadArray("records");
    QVector<QPair<QString,int>> records;
    records.append({userName, challengeScore});
    for (int i = 0; i < arrSize; ++i) {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        int score = settings.value("score").toInt();
        records.append({name, score});
    }
    settings.endArray();

    std::sort(records.begin(), records.end(),
              [](const QPair<QString,int>& a, const QPair<QString,int>& b){
                  return a.second > b.second;
              });
    if (records.size() > 10) records.resize(10);

    settings.beginWriteArray("records");
    for (int i = 0; i < records.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", records[i].first);
        settings.setValue("score", records[i].second);
        settings.setValue("date",
                          QDateTime::currentDateTime().toString("yyyy-MM-dd"));
    }
    settings.endArray();
    settings.endGroup();
}

void MainWindow::showChallengeRanking()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("挑战赛排行榜"));
    dlg.resize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QTableWidget *table = new QTableWidget(&dlg);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(
        {tr("排名"), tr("玩家"), tr("得分"), tr("日期")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    QSettings settings(saveFilePath, QSettings::IniFormat);
    settings.beginGroup("ChallengeRanking");
    int arrSize = settings.beginReadArray("records");
    table->setRowCount(arrSize);
    for (int i = 0; i < arrSize; ++i) {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        int score = settings.value("score").toInt();
        QString date = settings.value("date").toString();

        table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        table->setItem(i, 1, new QTableWidgetItem(name));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(score)));
        table->setItem(i, 3, new QTableWidgetItem(date));
    }
    settings.endArray();
    settings.endGroup();

    layout->addWidget(table);

    QPushButton *closeButton = new QPushButton(tr("关闭"), &dlg);
    connect(closeButton, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeButton);

    dlg.exec();
}
void MainWindow::toast(const QString &txt)
{
    auto *lab = new QLabel(txt, this);
    lab->setStyleSheet("QLabel{background:rgba(30,30,30,200);"
                       "color:#fff;padding:6px 12px;border-radius:6px;}");
    lab->adjustSize();
    lab->move(40,
              height()/2 - lab->height()/2);
    lab->show();
    QTimer::singleShot(2000, lab, &QLabel::deleteLater);
}


void MainWindow::saveStat()
{
    _cfg.beginGroup("Stat");
    _cfg.setValue("play", _stat.playSec);
    _cfg.setValue("clr",  _stat.cleared);
    _cfg.setValue("fast", _stat.fastestSec);
    _cfg.setValue("min3", _stat.min3);
    _cfg.setValue("min4", _stat.min4);
    _cfg.setValue("min5", _stat.min5);
    _cfg.endGroup();
    _cfg.sync();
}

void MainWindow::unlock(const QString &id,const QString &msg,bool cond)
{
    if (!cond || _ach.value(id)) return;
    _ach[id] = true;
    _cfg.beginGroup("Ach");
    _cfg.setValue(id,true);
    _cfg.endGroup();
    toast(tr("成就解锁：%1").arg(achName.value(msg,msg)));
}


void MainWindow::addPlaySec(int s)
{
    _stat.playSec += s;
    if (!(_stat.playSec % 60)) saveStat();          // 每整分钟落盘
    unlock("online1h", tr("成就·长情陪伴"), _stat.playSec >= 3600);
}


void MainWindow::levelFinishedHook(int size,int steps,int sec)
{
    _stat.cleared++;
    _stat.fastestSec = qMin(_stat.fastestSec, sec);
    if(size==3) _stat.min3 = qMin(_stat.min3, steps);
    if(size==4) _stat.min4 = qMin(_stat.min4, steps);
    if(size==5) _stat.min5 = qMin(_stat.min5, steps);
    if(size==6) _stat.min6 = qMin(_stat.min6, steps);

    saveStat();

    unlock("firstClear",  tr("成就·初窥门径"), _stat.cleared>=1);
    unlock("tenClear",    tr("成就·熟能生巧"), _stat.cleared>=10);
    unlock("clear4x4",    tr("成就·方寸之上"), size==4);
    unlock("clear5x5",    tr("成就·登峰造极"), size==5);
    unlock("clear6x6",    tr("成就·无双棋士"), size==6);
}


void MainWindow::challengeWinHook()
{
    unlock("challengeWin", tr("成就·挑战大师"), true);
}


