#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QImage>
#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QVector>

#include "databasemanager.h"

class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QTextEdit;
class QTableWidget;
class QSpinBox;
class QDateTimeEdit;
class QTimer;

enum class IssueStep {
    Idle,
    Issuing,
    SendingPhoto,
    SendingName,
    SendingDept,
    UpdatingImg,
    Verifying,
    Complete,
    Error
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void refreshPorts();
    void toggleSerial();
    void readSerialData();
    void choosePhoto();
    void readCard();
    void writeCard();
    void clearCard();
    void listAllRecords();
    void listRecentRecords();
    void clearForm();
    void searchRecords();
    void clearSearchFilter();
    void performLocalSearch();

private:
    void buildUi();
    void connectSignals();
    void initDatabase();
    void logMessage(const QString &message);
    void sendLine(const QString &line);
    void handleReceivedLine(const QString &line);
    void appendAttendanceRecord(const QString &line);
    void saveIssueRecord();
    bool loadIssueRecord(const QString &cid);
    void saveIssueImages(const QString &cid) const;
    QByteArray makePhotoPayload();
    QByteArray makeTextPayload(const QString &text, QLabel *previewLabel, QImage *previewImage);
    void sendImageBlocks(const QString &prefix, const QByteArray &payload, int blockCount);
    bool handleImageBlockLine(const QString &line);
    void resetReceivedImageBlocks();
    void updateImageFromBlocks();
    QImage unpackMonoImage(const QByteArray &payload, int width, int height) const;
    int selectedCardType() const;
    QString selectedPortName() const;
    QString typeText(int type) const;
    void displaySearchResults(const QList<AttendanceRecord> &records);

    // 人员管理
    void refreshPersonnelTable();
    void reportCardLost();
    void replaceCard();
    void completeReplaceCard(const QString &newUid);
    void modifyPersonnel();
    void deletePersonnel();
    void syncIssueRecordCsv();

    // 心跳处理
    void handleHeartbeat(const QString &line);
    void onHeartbeatTimeout();
    void updateDeviceStatusDisplay(const QString &dev, const QString &mode,
                                   const QString &temp, const QString &wifi,
                                   const QString &pend);

    // 发卡状态机
    void startIssueFlow();
    void continueIssueFlow();
    void abortIssueFlow(const QString &reason);
    void sendNextImageBlock();

    QSerialPort serial_;
    QByteArray receiveBuffer_;
    QString lastCommand_;
    QImage selectedPhoto_;
    QImage photoBinaryPreview_;
    QImage namePreview_;
    QImage deptPreview_;
    QByteArray receivedPhotoPayload_;
    QByteArray receivedNamePayload_;
    QByteArray receivedDeptPayload_;
    QVector<bool> receivedPhotoBlocks_;
    QVector<bool> receivedNameBlocks_;
    QVector<bool> receivedDeptBlocks_;
    bool readImageRequested_ = false;

    // 发卡状态机成员
    IssueStep issueStep_ = IssueStep::Idle;
    int issueBlockIndex_ = 0;
    QByteArray issuePhotoPayload_;
    QByteArray issueNamePayload_;
    QByteArray issueDeptPayload_;
    int issueCardType_ = 0;
    QString issueCid_;          // 保存发卡字段，避免 clearForm() 后丢失
    QString issueSid_;
    QString issuePoints_;
    QString issueName_;
    QString issueDept_;

    QComboBox *portCombo_ = nullptr;
    QComboBox *baudCombo_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *openButton_ = nullptr;
    QTextEdit *serialLog_ = nullptr;

    QLineEdit *cardEdit_ = nullptr;
    QLineEdit *studentEdit_ = nullptr;
    QLineEdit *pointsEdit_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QLineEdit *deptEdit_ = nullptr;
    QComboBox *cardTypeCombo_ = nullptr;
    QLabel *photoPreview_ = nullptr;
    QLabel *namePreviewLabel_ = nullptr;
    QLabel *deptPreviewLabel_ = nullptr;

    QPushButton *choosePhotoButton_ = nullptr;
    QPushButton *readButton_ = nullptr;
    QPushButton *writeButton_ = nullptr;
    QPushButton *clearCardButton_ = nullptr;
    QLabel *cardHintLabel_ = nullptr;
    QPushButton *listAllButton_ = nullptr;
    QPushButton *listRecentButton_ = nullptr;
    QSpinBox *recentCountSpin_ = nullptr;
    QPushButton *clearFormButton_ = nullptr;

    QTableWidget *attendanceTable_ = nullptr;

    // 记录查询控件
    QDateTimeEdit *searchStartTime_ = nullptr;
    QDateTimeEdit *searchEndTime_ = nullptr;
    QLineEdit *searchSidEdit_ = nullptr;
    QLineEdit *searchNameEdit_ = nullptr;
    QLineEdit *searchDeptEdit_ = nullptr;
    QPushButton *searchButton_ = nullptr;
    QPushButton *clearSearchButton_ = nullptr;
    QLabel *searchResultLabel_ = nullptr;

    // 人员管理控件
    QTableWidget *personnelTable_ = nullptr;
    QPushButton *lostCardButton_ = nullptr;
    QPushButton *replaceCardButton_ = nullptr;
    QPushButton *modifyPersonButton_ = nullptr;
    QPushButton *deletePersonButton_ = nullptr;

    // ── 心跳 / 设备状态 ──
    QLabel *heartbeatStatusLabel_ = nullptr;
    QLabel *devIdLabel_ = nullptr;
    QLabel *devModeLabel_ = nullptr;
    QLabel *devTempLabel_ = nullptr;
    QLabel *devWifiLabel_ = nullptr;
    QLabel *devPendLabel_ = nullptr;
    QTimer *heartbeatTimer_ = nullptr;
    static constexpr int HeartbeatTimeoutMs = 8000;  // 8秒无心跳即判离线

    // 搜索前是否正在等待设备同步
    bool pendingDeviceSyncForSearch_ = false;

    // LIST 同步进行中标记（历史记录回放时不发 DENY）
    bool isListSyncActive_ = false;

    // 补卡状态（等待读卡）
    bool pendingReplaceCard_ = false;
    int pendingReplaceOldId_ = 0;
    QString pendingReplaceOldUid_;
    IssueRecord pendingReplaceOldRec_;
};

#endif
