//mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "solver_util.h"
#include"poemrepository.h"
#include <QMainWindow>
#include <QPushButton>
#include <QMap>
#include <QStringList>
#include <QSettings>
#include<QElapsedTimer>
#include<QTimer>
#include<QVBoxLayout>
#include<QStack>
#include<QLabel>
using Grid=QVector<QVector<QString>>;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
struct Stat {
    int playSec     = 0;     // 累计在线秒
    int cleared     = 0;     // 通关诗句
    int fastestSec  = 9999;  // 单局最快
    int min3        = 9999;  // 3×3 最少步
    int min4        = 9999;  // 4×4
    int min5        = 9999;  // 5×5
};



class MainWindow : public QMainWindow {
    Q_OBJECT;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateTimeLabel();
    void handleButtonClicked();
    void showInstruction();
    void saveGame(bool showMsg=true,bool writeRanking=true);
    void loadGame();
    void clearGame();
    void showRanking();
    void showGlobalRanking();
    void autoSolve();
    void onSolverStep(const Step& step);

    void resetClock();        // 归零并停表
    void undo();//ctrl+Z

    void togglePause();
    void showHint();
    void startChallengeMode();
    void stopChallengeMode(bool success);
    void updateChallengeTimer();
    void loadChallengePoems();
    void nextChallengePoem();
    void saveChallengeResult();
    void showChallengeRanking();
    void loadGame(const QString& filePath);


private:
    Ui::MainWindow *ui;
    int gridSize=3;
    qint64 elapsedAcc=0;
    QVector<QVector<QPushButton*>> buttons;
    QString userName;

    QPair<int, int> firstClick = {-1, -1};


    Grid goal;//目标顺序
    Grid current;//当前顺序

    QString saveFilePath;
    int moveCount=0;
    QElapsedTimer timeCounter;//记录开始时间
    QTimer* updateTimer;//刷新时间

    bool isSolved();//判断是否完成题目
    void initUI();
    void updateGrid();
    void swapRows(int r1, int r2);
    void swapCols(int c1, int c2);
    void buildPoemMap();
    void loadPoem(const QString& title);
    void loadPoemFromFile(const QString&filename);
    void restartCurrentPoem();

    void initGridUI();
    void changeGridSize(const QString& text);

    QVector<Step>greedySolve(const QVector<QVector<QString>>& start,const QVector<QVector<QString>>& target,int N);
    void applyStep(const Step& s,bool recordUndo=true);
    QVector<Step> pendingSteps;
    QTimer       *playTimer {nullptr};

    void playNextStep();
    QVector<Step> solveRowCol(const Grid& cur, const Grid& goal, int N);
    void togglenight(bool on);
    QVBoxLayout *toolLayout=nullptr;
    void highlightButton(int row, int col, bool on);

    bool          gameRunning{false}; // 计时器是否在跑
    bool          autoSolving{false}; // 自动求解中？
    void           restartCurrent();
    QStack<Step> undoStack;
    QStack<Step>redoStack;
    QWidget *pauseCover{nullptr};
    void highlightStep(const Step &s);
    QVector<Step> nextHint();
    bool gameStarted{false};

    static inline void swapLinesOrCols(MainWindow *w, const Step &s)

    {
        if (s.isRow)
            w->swapRows(s.a, s.b);   // 函数已经存在
        else
            w->swapCols(s.a, s.b);   // 函数已经存在
    }
    bool challengeMode = false;            // 是否处于挑战赛模式
    int challengeTimeLimit = 180;          // 挑战赛时间限制（秒），默认 3 分钟
    int challengeRemainingTime = 0;        // 挑战赛剩余时间
    QTimer *challengeTimer = nullptr;      // 挑战赛倒计时器
    QVector<Poem> challengePoems;          // 挑战赛专用诗词库
    int challengeScore = 0;                // 挑战赛累计得分
    QLabel *challengeInfo = nullptr;       // 状态栏中显示“挑战赛模式 - 剩余时间 / 得分”

    // 辅助：格式化秒数为 mm:ss
    QString formatTime(int seconds);
    QList<QPushButton*> sideButtons;
    Stat              _stat;   // 统计数据
    QMap<QString,bool> _ach;   // 已解锁成就
    QSettings         _cfg;    // achievement.ini
    QElapsedTimer     _secTick;// 在线秒数计时
    void toast(const QString& txt);
    void saveStat();
    void addPlaySec(int s);
    void levelFinishedHook(int size,int steps,int sec);
    void challengeWinHook();
    void unlock(const QString& id, const QString& msg, bool cond);
    QPushButton *exitChallengeBtn{nullptr};
    const QMap<QString, QString> achName{
        {"firstClear" ,  tr("初窥门径")},
        {"tenClear"   ,  tr("熟能生巧")},
        {"clear4x4"   ,  tr("方寸之上")},
        {"clear5x5"   ,  tr("登峰造极")},
        {"online1h"   ,  tr("长情陪伴")},
        {"challengeWin", tr("挑战大师")}
    };
    int autoCnt=0;
    const int kAutoTHreshold=5;
    void autoQuickSave();
    void closeEvent(QCloseEvent *e)override;
    bool autoRound=false;
    bool quickSaveTick=false;

};
#endif // MAINWINDOW_H
