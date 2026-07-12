#include "mainwindow.h"
#include "databasemanager.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <initializer_list>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#include <QTableWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QVector>

namespace {

const int PhotoWidth = 48;
const int PhotoHeight = 64;
const int TextWidth = 80;
const int TextHeight = 16;
const int BytesPerBlock = 16;
const int PhotoBlockCount = 24;
const int TextBlockCount = 10;

QString requireText(QLineEdit *edit, const QString &fieldName)
{
    const QString text = edit->text().trimmed();
    if (text.isEmpty()) {
        throw fieldName;
    }
    return text;
}

bool isUidHex(const QString &uid)
{
    static const QRegularExpression regex("^[0-9A-Fa-f]{8}$");
    return regex.match(uid).hasMatch();
}

bool isDecimalText(const QString &text)
{
    static const QRegularExpression regex("^\\d+$");
    return regex.match(text).hasMatch();
}

bool isEightDigitStudentId(const QString &text)
{
    static const QRegularExpression regex("^\\d{8}$");
    return regex.match(text).hasMatch();
}

QString issueRecordPath()
{
    return QCoreApplication::applicationDirPath() + "/issue_records.csv";
}

QString issueImageDir()
{
    return QCoreApplication::applicationDirPath() + "/issued_cards";
}

QByteArray packMonoImage(const QImage &image)
{
    QByteArray payload;
    payload.reserve(image.width() * image.height() / 8);

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); x += 8) {
            uchar byte = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int gray = qGray(image.pixel(x + bit, y));
                if (gray < 128) {
                    byte |= static_cast<uchar>(1 << (7 - bit));
                }
            }
            payload.append(static_cast<char>(byte));
        }
    }

    return payload;
}

QTableWidgetItem *cell(const QString &text)
{
    return new QTableWidgetItem(text.trimmed());
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    connectSignals();
    resetReceivedImageBlocks();
    refreshPorts();
    initDatabase();
}

void MainWindow::buildUi()
{
    setWindowTitle("NFC考勤系统上位机");
    resize(1220, 800);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *title = new QLabel("NFC考勤系统上位机", central);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    auto *teamInfo = new QLabel("课程名称：专业实践综合设计II    小组成员：张烨23041035，杜昱辰23041036", central);
    QFont teamFont = teamInfo->font();
    teamFont.setPointSize(16);
    teamFont.setBold(true);
    teamInfo->setFont(teamFont);
    teamInfo->setAlignment(Qt::AlignCenter);

    root->addWidget(title);
    root->addWidget(teamInfo);

    auto *mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(12);
    root->addLayout(mainLayout, 1);

    auto *leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(12);
    mainLayout->addLayout(leftPanel, 2);

    auto *serialGroup = new QGroupBox("串口通信", central);
    auto *serialLayout = new QGridLayout(serialGroup);
    portCombo_ = new QComboBox(serialGroup);
    baudCombo_ = new QComboBox(serialGroup);
    refreshButton_ = new QPushButton("刷新串口", serialGroup);
    openButton_ = new QPushButton("打开串口", serialGroup);
    baudCombo_->addItems({"9600", "19200", "38400", "57600", "115200"});
    baudCombo_->setCurrentText("115200");
    serialLayout->addWidget(new QLabel("串口号"), 0, 0);
    serialLayout->addWidget(portCombo_, 0, 1);
    serialLayout->addWidget(new QLabel("波特率"), 0, 2);
    serialLayout->addWidget(baudCombo_, 0, 3);
    serialLayout->addWidget(refreshButton_, 0, 4);
    serialLayout->addWidget(openButton_, 0, 5);

    serialLog_ = new QTextEdit(serialGroup);
    serialLog_->setReadOnly(true);
    serialLog_->setMinimumHeight(170);
    serialLayout->addWidget(serialLog_, 1, 0, 1, 6);
    leftPanel->addWidget(serialGroup);

    // ── 心跳 / 设备状态 ──
    auto *heartbeatGroup = new QGroupBox("设备状态", central);
    auto *heartbeatLayout = new QGridLayout(heartbeatGroup);
    heartbeatStatusLabel_ = new QLabel("● 离线", heartbeatGroup);
    heartbeatStatusLabel_->setStyleSheet("color: red; font-weight: bold;");
    devIdLabel_   = new QLabel("--", heartbeatGroup);
    devModeLabel_ = new QLabel("--", heartbeatGroup);
    devTempLabel_ = new QLabel("--", heartbeatGroup);
    devWifiLabel_ = new QLabel("--", heartbeatGroup);
    devPendLabel_ = new QLabel("--", heartbeatGroup);
    heartbeatLayout->addWidget(new QLabel("状态"), 0, 0);
    heartbeatLayout->addWidget(heartbeatStatusLabel_, 0, 1);
    heartbeatLayout->addWidget(new QLabel("设备"), 0, 2);
    heartbeatLayout->addWidget(devIdLabel_, 0, 3);
    heartbeatLayout->addWidget(new QLabel("模式"), 1, 0);
    heartbeatLayout->addWidget(devModeLabel_, 1, 1);
    heartbeatLayout->addWidget(new QLabel("温度"), 1, 2);
    heartbeatLayout->addWidget(devTempLabel_, 1, 3);
    heartbeatLayout->addWidget(new QLabel("WiFi"), 2, 0);
    heartbeatLayout->addWidget(devWifiLabel_, 2, 1);
    heartbeatLayout->addWidget(new QLabel("待发"), 2, 2);
    heartbeatLayout->addWidget(devPendLabel_, 2, 3);
    leftPanel->addWidget(heartbeatGroup);

    auto *infoGroup = new QGroupBox("人员信息录入", central);
    auto *infoLayout = new QGridLayout(infoGroup);
    cardEdit_ = new QLineEdit(infoGroup);
    studentEdit_ = new QLineEdit(infoGroup);
    pointsEdit_ = new QLineEdit("0", infoGroup);
    nameEdit_ = new QLineEdit(infoGroup);
    deptEdit_ = new QLineEdit(infoGroup);
    cardTypeCombo_ = new QComboBox(infoGroup);
    cardTypeCombo_->addItems({"普通卡", "图像卡", "管理员卡"});
    cardEdit_->setPlaceholderText("8位HEX UID，例如 A1B2C3D4");
    studentEdit_->setPlaceholderText("8位数字学号，例如 19195227");
    infoLayout->addWidget(new QLabel("CID卡号"), 0, 0);
    infoLayout->addWidget(cardEdit_, 0, 1);
    infoLayout->addWidget(new QLabel("8位学号"), 0, 2);
    infoLayout->addWidget(studentEdit_, 0, 3);
    infoLayout->addWidget(new QLabel("积分PTS"), 1, 0);
    infoLayout->addWidget(pointsEdit_, 1, 1);
    infoLayout->addWidget(new QLabel("卡类型"), 1, 2);
    infoLayout->addWidget(cardTypeCombo_, 1, 3);
    infoLayout->addWidget(new QLabel("姓名"), 2, 0);
    infoLayout->addWidget(nameEdit_, 2, 1);
    infoLayout->addWidget(new QLabel("部门"), 2, 2);
    infoLayout->addWidget(deptEdit_, 2, 3);
    leftPanel->addWidget(infoGroup);

    auto *previewGroup = new QGroupBox("图像数据预览", central);
    auto *previewLayout = new QGridLayout(previewGroup);
    photoPreview_ = new QLabel("48x64头像", previewGroup);
    namePreviewLabel_ = new QLabel("80x16姓名", previewGroup);
    deptPreviewLabel_ = new QLabel("80x16部门", previewGroup);
    for (QLabel *label : {photoPreview_, namePreviewLabel_, deptPreviewLabel_}) {
        label->setFrameShape(QFrame::Box);
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumSize(180, 70);
    }
    photoPreview_->setFixedSize(150, 200);
    choosePhotoButton_ = new QPushButton("选择头像图片", previewGroup);
    previewLayout->addWidget(photoPreview_, 0, 0, 3, 1);
    previewLayout->addWidget(choosePhotoButton_, 3, 0);
    previewLayout->addWidget(new QLabel("姓名图像"), 0, 1);
    previewLayout->addWidget(namePreviewLabel_, 1, 1);
    previewLayout->addWidget(new QLabel("部门图像"), 2, 1);
    previewLayout->addWidget(deptPreviewLabel_, 3, 1);
    leftPanel->addWidget(previewGroup);

    auto *buttonLayout = new QGridLayout();
    readButton_ = new QPushButton("读卡", central);
    writeButton_ = new QPushButton("发卡写卡", central);
    clearCardButton_ = new QPushButton("清卡", central);
    listAllButton_ = new QPushButton("查询全部记录", central);
    listRecentButton_ = new QPushButton("查询最近N条", central);
    recentCountSpin_ = new QSpinBox(central);
    recentCountSpin_->setRange(1, 999);
    recentCountSpin_->setValue(10);
    clearFormButton_ = new QPushButton("清空界面", central);
    buttonLayout->addWidget(readButton_, 0, 0);
    buttonLayout->addWidget(writeButton_, 0, 1);
    buttonLayout->addWidget(clearCardButton_, 0, 2);
    buttonLayout->addWidget(listAllButton_, 1, 0);
    buttonLayout->addWidget(recentCountSpin_, 1, 1);
    buttonLayout->addWidget(listRecentButton_, 1, 2);
    buttonLayout->addWidget(clearFormButton_, 1, 3);
    leftPanel->addLayout(buttonLayout);

    cardHintLabel_ = new QLabel("提示：如需补卡/重绑UID，请读取或输入新卡UID后，再点击\u201c发卡写卡\u201d重新绑定人员信息。", central);
    QFont hintFont = cardHintLabel_->font();
    hintFont.setPointSize(8);
    cardHintLabel_->setFont(hintFont);
    cardHintLabel_->setStyleSheet("color: #888888;");
    cardHintLabel_->setWordWrap(true);
    leftPanel->addWidget(cardHintLabel_);

    auto *rightPanel = new QVBoxLayout();
    mainLayout->addLayout(rightPanel, 3);

    // ── 记录查询 ──
    auto *searchGroup = new QGroupBox("记录查询（本地数据库）", central);
    auto *searchLayout = new QGridLayout(searchGroup);
    searchLayout->setSpacing(6);

    searchStartTime_ = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7), searchGroup);
    searchEndTime_   = new QDateTimeEdit(QDateTime::currentDateTime(), searchGroup);
    searchStartTime_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    searchEndTime_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    searchStartTime_->setCalendarPopup(true);
    searchEndTime_->setCalendarPopup(true);

    searchSidEdit_  = new QLineEdit(searchGroup);
    searchNameEdit_ = new QLineEdit(searchGroup);
    searchDeptEdit_ = new QLineEdit(searchGroup);
    searchSidEdit_->setPlaceholderText("学号（模糊）");
    searchNameEdit_->setPlaceholderText("姓名（模糊）");
    searchDeptEdit_->setPlaceholderText("部门（模糊）");

    searchButton_       = new QPushButton("[查询]", searchGroup);
    clearSearchButton_  = new QPushButton("清空条件", searchGroup);
    searchResultLabel_  = new QLabel("", searchGroup);

    searchLayout->addWidget(new QLabel("起始时间"), 0, 0);
    searchLayout->addWidget(searchStartTime_, 0, 1);
    searchLayout->addWidget(new QLabel("截止时间"), 0, 2);
    searchLayout->addWidget(searchEndTime_, 0, 3);
    searchLayout->addWidget(new QLabel("学号"), 1, 0);
    searchLayout->addWidget(searchSidEdit_, 1, 1);
    searchLayout->addWidget(new QLabel("姓名"), 1, 2);
    searchLayout->addWidget(searchNameEdit_, 1, 3);
    searchLayout->addWidget(new QLabel("部门"), 2, 0);
    searchLayout->addWidget(searchDeptEdit_, 2, 1);
    searchLayout->addWidget(searchButton_, 2, 2);
    searchLayout->addWidget(clearSearchButton_, 2, 3);
    searchLayout->addWidget(searchResultLabel_, 3, 0, 1, 4);

    rightPanel->addWidget(searchGroup);

    // ── 人员管理 ──
    auto *personnelGroup = new QGroupBox("人员管理（挂失 / 补卡 / 修改 / 删除）", central);
    auto *personnelLayout = new QVBoxLayout(personnelGroup);

    personnelTable_ = new QTableWidget(0, 7, personnelGroup);
    personnelTable_->setHorizontalHeaderLabels({"卡号UID", "学号", "姓名", "部门", "卡类型", "积分", "状态"});
    personnelTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    personnelTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    personnelTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    personnelTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    personnelTable_->setMinimumHeight(150);
    personnelLayout->addWidget(personnelTable_);

    auto *personnelBtnLayout = new QHBoxLayout();
    lostCardButton_     = new QPushButton("挂失/解除", personnelGroup);
    replaceCardButton_  = new QPushButton("补卡", personnelGroup);
    modifyPersonButton_  = new QPushButton("修改信息", personnelGroup);
    deletePersonButton_  = new QPushButton("删除人员", personnelGroup);
    personnelBtnLayout->addWidget(lostCardButton_);
    personnelBtnLayout->addWidget(replaceCardButton_);
    personnelBtnLayout->addWidget(modifyPersonButton_);
    personnelBtnLayout->addWidget(deletePersonButton_);
    personnelLayout->addLayout(personnelBtnLayout);

    rightPanel->addWidget(personnelGroup);

    auto *attendanceGroup = new QGroupBox("考勤记录 / LIST响应", central);
    auto *attendanceLayout = new QVBoxLayout(attendanceGroup);
    attendanceTable_ = new QTableWidget(0, 5, attendanceGroup);
    attendanceTable_->setHorizontalHeaderLabels({"UID", "学号", "时间", "设备", "状态"});
    attendanceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    attendanceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    attendanceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    attendanceLayout->addWidget(attendanceTable_);
    rightPanel->addWidget(attendanceGroup);

    setCentralWidget(central);
}

void MainWindow::connectSignals()
{
    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(openButton_, &QPushButton::clicked, this, &MainWindow::toggleSerial);
    connect(&serial_, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
    connect(choosePhotoButton_, &QPushButton::clicked, this, &MainWindow::choosePhoto);
    connect(readButton_, &QPushButton::clicked, this, &MainWindow::readCard);
    connect(writeButton_, &QPushButton::clicked, this, &MainWindow::writeCard);
    connect(clearCardButton_, &QPushButton::clicked, this, &MainWindow::clearCard);
    connect(listAllButton_, &QPushButton::clicked, this, &MainWindow::listAllRecords);
    connect(listRecentButton_, &QPushButton::clicked, this, &MainWindow::listRecentRecords);
    connect(clearFormButton_, &QPushButton::clicked, this, &MainWindow::clearForm);
    connect(searchButton_, &QPushButton::clicked, this, &MainWindow::searchRecords);
    connect(clearSearchButton_, &QPushButton::clicked, this, &MainWindow::clearSearchFilter);
    connect(lostCardButton_, &QPushButton::clicked, this, &MainWindow::reportCardLost);
    connect(replaceCardButton_, &QPushButton::clicked, this, &MainWindow::replaceCard);
    connect(modifyPersonButton_, &QPushButton::clicked, this, &MainWindow::modifyPersonnel);
    connect(deletePersonButton_, &QPushButton::clicked, this, &MainWindow::deletePersonnel);

    connect(nameEdit_, &QLineEdit::textChanged, this, [this] {
        const QByteArray payload = makeTextPayload(nameEdit_->text(), namePreviewLabel_, &namePreview_);
        Q_UNUSED(payload);
    });
    connect(deptEdit_, &QLineEdit::textChanged, this, [this] {
        const QByteArray payload = makeTextPayload(deptEdit_->text(), deptPreviewLabel_, &deptPreview_);
        Q_UNUSED(payload);
    });

    // 心跳超时定时器
    heartbeatTimer_ = new QTimer(this);
    heartbeatTimer_->setSingleShot(true);
    connect(heartbeatTimer_, &QTimer::timeout, this, &MainWindow::onHeartbeatTimeout);
}

void MainWindow::refreshPorts()
{
    const QString previousPort = selectedPortName();
    portCombo_->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    int preferredIndex = -1;
    for (const QSerialPortInfo &port : ports) {
        QString text = port.portName();
        if (!port.description().isEmpty()) {
            text += " - " + port.description();
        }
        if (!port.manufacturer().isEmpty()) {
            text += " - " + port.manufacturer();
        }

        portCombo_->addItem(text, port.portName());

        const QString lower = text.toLower();
        if (preferredIndex < 0 && (lower.contains("ch340") || lower.contains("cp210")
                || lower.contains("usb") || lower.contains("stlink"))) {
            preferredIndex = portCombo_->count() - 1;
        }
        if (port.portName() == previousPort) {
            preferredIndex = portCombo_->count() - 1;
        }
    }

    if (preferredIndex >= 0) {
        portCombo_->setCurrentIndex(preferredIndex);
    }

    logMessage(QString("已发现 %1 个串口").arg(ports.size()));
}

void MainWindow::toggleSerial()
{
    if (serial_.isOpen()) {
        serial_.close();
        openButton_->setText("打开串口");
        logMessage("串口已关闭");
        return;
    }

    if (selectedPortName().isEmpty()) {
        QMessageBox::warning(this, "提示", "没有可用串口，请先连接设备并刷新。");
        return;
    }

    serial_.setPortName(selectedPortName());
    serial_.setBaudRate(baudCombo_->currentText().toInt());
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial_.open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, "串口打开失败", serial_.errorString());
        return;
    }

    openButton_->setText("关闭串口");
    logMessage("串口已打开：" + selectedPortName());
}

void MainWindow::readSerialData()
{
    receiveBuffer_ += serial_.readAll();

    int index = receiveBuffer_.indexOf('\n');
    while (index >= 0) {
        QByteArray rawLine = receiveBuffer_.left(index);
        receiveBuffer_.remove(0, index + 1);
        rawLine = rawLine.trimmed();

        if (!rawLine.isEmpty()) {
            handleReceivedLine(QString::fromLocal8Bit(rawLine));
        }

        index = receiveBuffer_.indexOf('\n');
    }
}

void MainWindow::choosePhoto()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "选择头像图片", QString(), "Images (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty()) {
        return;
    }

    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, "提示", "图片读取失败。");
        return;
    }

    selectedPhoto_ = image;
    const QByteArray payload = makePhotoPayload();
    Q_UNUSED(payload);
    logMessage("已选择头像：" + path);
}

void MainWindow::readCard()
{
    clearForm();
    sendLine("READ");
}

void MainWindow::writeCard()
{
    try {
        const QString cid = requireText(cardEdit_, "CID卡号").toUpper();
        const QString sid = requireText(studentEdit_, "8位学号");
        const QString points = pointsEdit_->text().trimmed().isEmpty() ? "0" : pointsEdit_->text().trimmed();
        const int cardType = selectedCardType();

        if (!isUidHex(cid)) {
            QMessageBox::warning(this, "提示", "CID卡号必须是8位HEX，例如 A1B2C3D4。");
            return;
        }
        if (!isEightDigitStudentId(sid)) {
            QMessageBox::warning(this, "提示", "学号必须是8位数字，例如 19195227。");
            return;
        }
        if (!isDecimalText(points)) {
            QMessageBox::warning(this, "提示", "PTS积分必须是十进制数字。");
            return;
        }

        // 准备图像数据
        QByteArray photoPayload(PhotoBlockCount * BytesPerBlock, '\0');
        QByteArray namePayload;
        QByteArray deptPayload;
        namePayload = makeTextPayload(requireText(nameEdit_, "姓名"), namePreviewLabel_, &namePreview_);
        deptPayload = makeTextPayload(requireText(deptEdit_, "部门"), deptPreviewLabel_, &deptPreview_);
        if (cardType == 1) {
            photoPayload = makePhotoPayload();
            if (photoPayload.isEmpty()) {
                QMessageBox::warning(this, "提示", "图像卡需要先选择真实头像图片。");
                return;
            }
        }

        // 保存到发卡状态机成员
        issuePhotoPayload_ = photoPayload;
        issueNamePayload_  = namePayload;
        issueDeptPayload_  = deptPayload;
        issueCardType_     = cardType;
        issueCid_          = cid;
        issueSid_          = sid;
        issuePoints_       = points;
        issueName_         = nameEdit_->text().trimmed();
        issueDept_         = deptEdit_->text().trimmed();

        // 启动发卡状态机
        startIssueFlow();

        // 第一步：下发 ISSUE 指令
        sendLine(QString("ISSUE:%1,%2,%3,%4").arg(cid, sid, points).arg(cardType));
    } catch (const QString &fieldName) {
        QMessageBox::warning(this, "提示", fieldName + "不能为空。");
    }
}

void MainWindow::clearCard()
{
    const QString cid = cardEdit_->text().trimmed().toUpper();
    if (!isUidHex(cid)) {
        QMessageBox::warning(this, "提示", "清卡需要填写8位HEX UID。");
        return;
    }
    sendLine("CLEAR:" + cid);
}

void MainWindow::listAllRecords()
{
    sendLine("LIST:ALL");
}

void MainWindow::listRecentRecords()
{
    sendLine(QString("LIST:%1").arg(recentCountSpin_->value()));
}

void MainWindow::clearForm()
{
    cardEdit_->clear();
    studentEdit_->clear();
    pointsEdit_->setText("0");
    nameEdit_->clear();
    deptEdit_->clear();
    selectedPhoto_ = QImage();
    photoBinaryPreview_ = QImage();
    namePreview_ = QImage();
    deptPreview_ = QImage();
    photoPreview_->setText("48x64头像");
    photoPreview_->setPixmap(QPixmap());
    namePreviewLabel_->setText("80x16姓名");
    namePreviewLabel_->setPixmap(QPixmap());
    deptPreviewLabel_->setText("80x16部门");
    deptPreviewLabel_->setPixmap(QPixmap());
}

void MainWindow::initDatabase()
{
    DatabaseManager &db = DatabaseManager::instance();
    if (db.open()) {
        const int count = db.recordCount();
        logMessage(QString("数据库已就绪，现有 %1 条考勤记录").arg(count));
        refreshPersonnelTable();
    } else {
        logMessage("警告：数据库初始化失败，考勤记录将无法持久化存储");
    }
}

void MainWindow::logMessage(const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    serialLog_->append(QString("[%1] %2").arg(time, message));
}

void MainWindow::sendLine(const QString &line)
{
    if (!serial_.isOpen()) {
        QMessageBox::warning(this, "提示", "请先打开串口。");
        return;
    }

    serial_.write(line.toLatin1() + '\n');
    lastCommand_ = line;
    logMessage("发送: " + line);
}

void MainWindow::handleReceivedLine(const QString &line)
{
    logMessage("接收: " + line);

    // ── 成功应答 ──
    if (line == "OK") {
        if (issueStep_ != IssueStep::Idle && issueStep_ != IssueStep::Complete && issueStep_ != IssueStep::Error) {
            continueIssueFlow();
        }
        return;
    }

    // ── 错误应答 ──
    if (line.startsWith("ERR:")) {
        QString errMsg;
        if (line == "ERR:PARSE")           errMsg = "下位机参数解析失败";
        else if (line == "ERR:NOCARD")     errMsg = "感应区未检测到卡片";
        else if (line == "ERR:CID_MISMATCH") errMsg = "指令UID与读卡UID不一致";
        else if (line == "ERR:AUTH")       errMsg = "RC522扇区密钥校验失败";
        else if (line == "ERR:WRITE_B1")   errMsg = "卡片Block1写入失败";
        else if (line == "ERR:IMG_PARSE")  errMsg = "图像块命令格式解析错误";
        else if (line == "ERR:IMG_HEX")    errMsg = "图像块十六进制数据格式不符";
        else if (line == "ERR:IMG_BLOCK")  errMsg = "图像分块序号超出范围";
        else if (line == "ERR:IMG_TYPE")   errMsg = "无法识别的图像类型(A/N/D)";
        else if (line == "ERR:UNKNOWN_CMD") errMsg = "下位机不支持该指令";
        else                               errMsg = "未知错误: " + line;

        logMessage("错误: " + errMsg);
        if (issueStep_ != IssueStep::Idle && issueStep_ != IssueStep::Complete) {
            abortIssueFlow(errMsg);
        }
        return;
    }

    // ── LIST 响应头部 ──
    if (line.startsWith("LIST:") && !line.startsWith("LIST:END")) {
        // LIST:N 或 LIST:ALL 响应头
        const QString countStr = line.mid(5).trimmed();
        logMessage("考勤记录总数: " + countStr);
        attendanceTable_->setRowCount(0);
        attendanceTable_->setColumnCount(5);
        attendanceTable_->setHorizontalHeaderLabels({"UID", "学号", "时间", "设备", "状态"});
        attendanceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        return;
    }

    // ── LIST 响应结束 ──
    if (line == "LIST:END") {
        const int totalRows = attendanceTable_->rowCount();
        logMessage("考勤记录传输完成，共 " + QString::number(totalRows) + " 条");

        // 如果是从搜索按钮触发的同步，自动执行本地筛选
        if (pendingDeviceSyncForSearch_) {
            pendingDeviceSyncForSearch_ = false;
            logMessage("设备同步完成，正在按条件筛选...");
            performLocalSearch();
        }
        return;
    }

    // ── READ 应答: UID / SID / PTS / TYPE ──
    if (line.startsWith("UID:")) {
        const QString uid = line.mid(4).trimmed().toUpper();
        cardEdit_->setText(uid);

        // 补卡流程：等待读取新卡 UID
        if (pendingReplaceCard_) {
            pendingReplaceCard_ = false;
            completeReplaceCard(uid);
        }
        return;
    }
    if (line.startsWith("SID:")) {
        studentEdit_->setText(line.mid(4).trimmed());
        return;
    }
    if (line.startsWith("PTS:")) {
        pointsEdit_->setText(line.mid(4).trimmed());
        return;
    }
    if (line.startsWith("TYPE:")) {
        const int type = line.mid(5).trimmed().toInt();
        if (type >= 0 && type < cardTypeCombo_->count()) {
            cardTypeCombo_->setCurrentIndex(type);
        }
        // 发卡验证流程：收到 TYPE 标志着 READ 验证完成
        if (issueStep_ == IssueStep::Verifying) {
            issueStep_ = IssueStep::Complete;
            saveIssueRecord();
            refreshPersonnelTable();
            logMessage("发卡流程完成，验证通过");
        }
        return;
    }

    // ── 心跳 ──
    if (line.startsWith("HEART:")) {
        handleHeartbeat(line);
        return;
    }

    // ── 考勤记录 ──
    if (line.startsWith("REC:")) {
        appendAttendanceRecord(line);
        return;
    }
}

void MainWindow::appendAttendanceRecord(const QString &line)
{
    QString body = line.mid(4);
    const QStringList fields = body.split('|');

    QString seq;
    QString uid;
    QString sid;
    QString type;
    QString rawTime;
    QString dev;
    QString status;

    for (const QString &field : fields) {
        if (field.startsWith("SEQ=")) {
            seq = field.mid(4);
        } else if (field.startsWith("UID=")) {
            uid = field.mid(4);
        } else if (field.startsWith("SID=")) {
            sid = field.mid(4);
        } else if (field.startsWith("DEV=")) {
            dev = field.mid(4);
        } else if (field.startsWith("TYPE")) {
            type = field;
        } else if (field.contains('-') && field.contains(':')) {
            rawTime = field;
        } else {
            status = field.trimmed();
        }
    }

    // ── 校准统一时间：上位机当前系统时间 ──
    const QString calibratedTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // ── 检查卡是否处于挂失/注销状态，若是则拒绝该记录 ──
    DatabaseManager &db = DatabaseManager::instance();
    IssueRecord issueRec;
    bool cardIsBlocked = false;
    QString blockedReason;
    if (db.fetchIssueRecord(uid, issueRec)) {
        if (issueRec.status == "lost") {
            cardIsBlocked = true;
            blockedReason = "挂失";
            status = "INV";  // 覆写为无效卡
        } else if (issueRec.status == "cancelled") {
            cardIsBlocked = true;
            blockedReason = "已注销";
            status = "INV";  // 覆写为无效卡
        }
    }

    // ── 持久化到 SQLite 数据库 ──
    AttendanceRecord dbRecord;
    dbRecord.seq            = seq;
    dbRecord.uid            = uid;
    dbRecord.sid            = sid;
    dbRecord.cardType       = type;
    dbRecord.rawTime        = rawTime;
    dbRecord.calibratedTime = calibratedTime;
    dbRecord.device         = dev;
    dbRecord.status         = status;

    if (db.isOpen()) {
        QString dbError;
        if (db.insertAttendanceRecord(dbRecord, &dbError)) {
            if (cardIsBlocked) {
                logMessage(QString("⚠ 拒绝刷卡: UID=%1 处于%2状态，已标记为无效卡 (INV)")
                    .arg(uid, blockedReason));
            } else {
                logMessage(QString("考勤记录已写入数据库 (UID=%1, 原始时间=%2, 校准时间=%3)")
                    .arg(uid, rawTime, calibratedTime));
            }
        } else {
            logMessage("警告：考勤记录写入数据库失败 → " + dbError);
        }
    } else {
        logMessage("警告：数据库未打开，考勤记录未能持久化！请重启程序。");
    }

    // ── 显示到表格（使用下位机原始时间） ──
    QString formattedTime = rawTime;
    const QDateTime dt = QDateTime::fromString(rawTime, "yyyy-MM-dd HH:mm:ss");
    if (dt.isValid()) {
        formattedTime = dt.toString("yyyy年MM月dd日 HH:mm:ss");
    }

    // 格式化设备：01 → 编号01
    const QString formattedDev = dev.isEmpty() ? dev : QString("编号%1").arg(dev);

    // 格式化状态
    QString formattedStatus = status;
    if (status == "IN")           formattedStatus = "签到";
    else if (status == "OUT")     formattedStatus = "离开";
    else if (status == "DUP")     formattedStatus = "重复刷卡";
    else if (status == "NOIN")    formattedStatus = "无入场离开";
    else if (status == "INV")     formattedStatus = "无效卡";

    // 补充挂失/注销原因标注
    if (cardIsBlocked) {
        formattedStatus += QString(" [%1]").arg(blockedReason);
    }

    const int row = attendanceTable_->rowCount();
    attendanceTable_->insertRow(row);
    attendanceTable_->setItem(row, 0, cell(uid));
    attendanceTable_->setItem(row, 1, cell(sid));
    attendanceTable_->setItem(row, 2, cell(formattedTime));
    attendanceTable_->setItem(row, 3, cell(formattedDev));
    attendanceTable_->setItem(row, 4, cell(formattedStatus));
}

// ═══════════════════════════════════════════════════
// 发卡状态机
// ═══════════════════════════════════════════════════

void MainWindow::startIssueFlow()
{
    issueStep_ = IssueStep::Issuing;
    issueBlockIndex_ = 0;
    logMessage("发卡流程启动 → 步骤1: ISSUE 写账户头");
}

void MainWindow::continueIssueFlow()
{
    switch (issueStep_) {
    case IssueStep::Issuing:
        // ISSUE 成功 → 步骤2~3: 发送头像 IMGA00~IMGA23
        issueStep_ = IssueStep::SendingPhoto;
        issueBlockIndex_ = 0;
        logMessage("步骤2: 开始发送头像图像 (IMGA00~IMGA23)");
        sendNextImageBlock();
        break;

    case IssueStep::SendingPhoto:
        issueBlockIndex_++;
        if (issueBlockIndex_ < PhotoBlockCount) {
            sendNextImageBlock();
        } else {
            // 头像发送完毕 → 步骤4~5: 发送姓名 IMGN00~IMGN09
            issueStep_ = IssueStep::SendingName;
            issueBlockIndex_ = 0;
            logMessage("步骤4: 开始发送姓名图像 (IMGN00~IMGN09)");
            sendNextImageBlock();
        }
        break;

    case IssueStep::SendingName:
        issueBlockIndex_++;
        if (issueBlockIndex_ < TextBlockCount) {
            sendNextImageBlock();
        } else {
            // 姓名发送完毕 → 步骤6~7: 发送部门 IMGD00~IMGD09
            issueStep_ = IssueStep::SendingDept;
            issueBlockIndex_ = 0;
            logMessage("步骤6: 开始发送部门图像 (IMGD00~IMGD09)");
            sendNextImageBlock();
        }
        break;

    case IssueStep::SendingDept:
        issueBlockIndex_++;
        if (issueBlockIndex_ < TextBlockCount) {
            sendNextImageBlock();
        } else {
            // 部门发送完毕 → 步骤8: UPDATEIMG
            issueStep_ = IssueStep::UpdatingImg;
            logMessage("步骤8: 执行 UPDATEIMG 写入图像");
            sendLine("UPDATEIMG");
        }
        break;

    case IssueStep::UpdatingImg:
        // UPDATEIMG 成功 → 步骤9: READ 验证
        issueStep_ = IssueStep::Verifying;
        clearForm();
        logMessage("步骤9: 读卡验证 (READ)");
        sendLine("READ");
        break;

    case IssueStep::Verifying:
    case IssueStep::Complete:
    case IssueStep::Idle:
    case IssueStep::Error:
        break;
    }
}

void MainWindow::abortIssueFlow(const QString &reason)
{
    issueStep_ = IssueStep::Error;
    logMessage("发卡流程中止: " + reason);
    QMessageBox::warning(this, "发卡失败", "发卡流程中止:\n" + reason);
}

void MainWindow::sendNextImageBlock()
{
    QString prefix;
    const QByteArray *payload = nullptr;
    int blockCount = 0;

    switch (issueStep_) {
    case IssueStep::SendingPhoto:
        prefix = QStringLiteral("IMGA");
        payload = &issuePhotoPayload_;
        blockCount = PhotoBlockCount;
        break;
    case IssueStep::SendingName:
        prefix = QStringLiteral("IMGN");
        payload = &issueNamePayload_;
        blockCount = TextBlockCount;
        break;
    case IssueStep::SendingDept:
        prefix = QStringLiteral("IMGD");
        payload = &issueDeptPayload_;
        blockCount = TextBlockCount;
        break;
    default:
        return;
    }

    const int i = issueBlockIndex_;
    if (i < 0 || i >= blockCount) {
        abortIssueFlow("图像块序号越界: " + QString::number(i));
        return;
    }

    const QByteArray block = payload->mid(i * BytesPerBlock, BytesPerBlock);
    sendLine(QString("%1%2:%3")
        .arg(prefix)
        .arg(i, 2, 10, QChar('0'))
        .arg(QString::fromLatin1(block.toHex().toUpper())));
}

void MainWindow::saveIssueRecord()
{
    const QString cid = issueCid_.toUpper();
    saveIssueImages(cid);

    qDebug() << "saveIssueRecord: cid=" << cid << "sid=" << issueSid_
             << "name=" << issueName_ << "dept=" << issueDept_
             << "points=" << issuePoints_ << "type=" << issueCardType_;

    // 写入数据库（本地信息库）
    IssueRecord rec;
    rec.uid        = cid;
    rec.sid        = issueSid_;
    rec.points     = issuePoints_;
    rec.name       = issueName_;
    rec.department = issueDept_;
    rec.cardType   = typeText(issueCardType_);

    QString dbError;
    bool ok = DatabaseManager::instance().insertOrUpdateIssueRecord(rec, &dbError);
    logMessage(QString("发卡记录%1: CID=%2 学号=%3 姓名=%4 部门=%5%6")
        .arg(ok ? "已保存" : "保存失败", cid, issueSid_, issueName_, issueDept_,
             ok ? "" : " → " + dbError));

    // CSV 备份
    syncIssueRecordCsv();
}

bool MainWindow::loadIssueRecord(const QString &cid)
{
    // 优先从数据库查询
    IssueRecord rec;
    if (DatabaseManager::instance().fetchIssueRecord(cid, rec)) {
        studentEdit_->setText(rec.sid);
        pointsEdit_->setText(rec.points);
        const int typeIndex = cardTypeCombo_->findText(rec.cardType);
        if (typeIndex >= 0)
            cardTypeCombo_->setCurrentIndex(typeIndex);
        nameEdit_->setText(rec.name);
        deptEdit_->setText(rec.department);
    } else {
        // 降级：从 CSV 备份读取
        QFile file(issueRecordPath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;

        QString matchedLine;
        QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        in.setCodec("UTF-8");
#else
        in.setEncoding(QStringConverter::Utf8);
#endif
        while (!in.atEnd()) {
            const QString line = in.readLine();
            const QStringList fields = line.split(',');
            if (fields.size() >= 7 && fields.at(1).compare(cid, Qt::CaseInsensitive) == 0)
                matchedLine = line;
        }

        if (matchedLine.isEmpty())
            return false;

        const QStringList fields = matchedLine.split(',');
        studentEdit_->setText(fields.at(2));
        pointsEdit_->setText(fields.at(3));
        const int typeIndex = cardTypeCombo_->findText(fields.at(4));
        if (typeIndex >= 0)
            cardTypeCombo_->setCurrentIndex(typeIndex);
        nameEdit_->setText(fields.at(5));
        deptEdit_->setText(fields.at(6));
    }

    const QString base = issueImageDir() + "/" + cid;
    QImage photo(base + "_photo.png");
    QImage nameImage(base + "_name.png");
    QImage deptImage(base + "_dept.png");

    if (!photo.isNull()) {
        photoBinaryPreview_ = photo;
        photoPreview_->setPixmap(QPixmap::fromImage(photo).scaled(
            photoPreview_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    if (!nameImage.isNull()) {
        namePreview_ = nameImage;
        namePreviewLabel_->setPixmap(QPixmap::fromImage(nameImage).scaled(
            namePreviewLabel_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    if (!deptImage.isNull()) {
        deptPreview_ = deptImage;
        deptPreviewLabel_->setPixmap(QPixmap::fromImage(deptImage).scaled(
            deptPreviewLabel_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }

    return true;
}

void MainWindow::saveIssueImages(const QString &cid) const
{
    if (cid.isEmpty()) {
        return;
    }

    QDir().mkpath(issueImageDir());
    const QString base = issueImageDir() + "/" + cid;
    if (!photoBinaryPreview_.isNull()) {
        photoBinaryPreview_.save(base + "_photo.png");
    }
    if (!namePreview_.isNull()) {
        namePreview_.save(base + "_name.png");
    }
    if (!deptPreview_.isNull()) {
        deptPreview_.save(base + "_dept.png");
    }
}

QByteArray MainWindow::makePhotoPayload()
{
    if (selectedPhoto_.isNull()) {
        return {};
    }

    const QImage scaled = selectedPhoto_.scaled(
        PhotoWidth, PhotoHeight, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int left = (scaled.width() - PhotoWidth) / 2;
    const int top = (scaled.height() - PhotoHeight) / 2;
    QImage mono = scaled.copy(left, top, PhotoWidth, PhotoHeight).convertToFormat(QImage::Format_RGB32);

    for (int y = 0; y < mono.height(); ++y) {
        for (int x = 0; x < mono.width(); ++x) {
            const int gray = qGray(mono.pixel(x, y));
            mono.setPixel(x, y, gray < 128 ? qRgb(0, 0, 0) : qRgb(255, 255, 255));
        }
    }

    photoBinaryPreview_ = mono;
    photoPreview_->setPixmap(QPixmap::fromImage(mono).scaled(
        photoPreview_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));

    return packMonoImage(mono);
}

QByteArray MainWindow::makeTextPayload(const QString &text, QLabel *previewLabel, QImage *previewImage)
{
    const QString trimmed = text.trimmed();

    // 自动适配字号：确保文字宽度在 80px 以内
    int fontSize = 10;
    QFont font("Microsoft YaHei");
    font.setBold(true);

    if (!trimmed.isEmpty()) {
        while (fontSize >= 7) {
            font.setPointSize(fontSize);
            QFontMetrics fm(font);
            if (fm.horizontalAdvance(trimmed) <= TextWidth - 4) {
                break;
            }
            --fontSize;
        }
    }

    QImage image(TextWidth, TextHeight, QImage::Format_RGB32);
    image.fill(Qt::white);

    if (!trimmed.isEmpty()) {
        QPainter painter(&image);
        painter.setPen(Qt::black);
        painter.setFont(font);
        painter.drawText(image.rect(), Qt::AlignCenter, trimmed);
        painter.end();
    }

    // 二值化：阈值 128，与 packMonoImage 一致
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int gray = qGray(image.pixel(x, y));
            image.setPixel(x, y, gray < 128 ? qRgb(0, 0, 0) : qRgb(255, 255, 255));
        }
    }

    *previewImage = image;
    previewLabel->setPixmap(QPixmap::fromImage(image).scaled(
        previewLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));

    return packMonoImage(image);
}

void MainWindow::sendImageBlocks(const QString &prefix, const QByteArray &payload, int blockCount)
{
    if (payload.size() != blockCount * BytesPerBlock) {
        logMessage(QString("图像数据长度错误: %1").arg(prefix));
        return;
    }

    for (int i = 0; i < blockCount; ++i) {
        const QByteArray block = payload.mid(i * BytesPerBlock, BytesPerBlock);
        sendLine(QString("%1%2:%3")
            .arg(prefix)
            .arg(i, 2, 10, QChar('0'))
            .arg(QString::fromLatin1(block.toHex().toUpper())));
    }
}

bool MainWindow::handleImageBlockLine(const QString &line)
{
    static const QRegularExpression regex("^(IMGA|IMGN|IMGD)(\\d{2}):([0-9A-Fa-f]{32})$");
    const QRegularExpressionMatch match = regex.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    const QString prefix = match.captured(1);
    const int index = match.captured(2).toInt();
    const QByteArray block = QByteArray::fromHex(match.captured(3).toLatin1());

    QByteArray *payload = nullptr;
    QVector<bool> *flags = nullptr;
    int blockCount = 0;
    if (prefix == "IMGA") {
        payload = &receivedPhotoPayload_;
        flags = &receivedPhotoBlocks_;
        blockCount = PhotoBlockCount;
    } else if (prefix == "IMGN") {
        payload = &receivedNamePayload_;
        flags = &receivedNameBlocks_;
        blockCount = TextBlockCount;
    } else {
        payload = &receivedDeptPayload_;
        flags = &receivedDeptBlocks_;
        blockCount = TextBlockCount;
    }

    if (index < 0 || index >= blockCount || block.size() != BytesPerBlock) {
        logMessage("收到无效图像块: " + line);
        return true;
    }

    payload->replace(index * BytesPerBlock, BytesPerBlock, block);
    (*flags)[index] = true;
    updateImageFromBlocks();
    return true;
}

void MainWindow::resetReceivedImageBlocks()
{
    receivedPhotoPayload_ = QByteArray(PhotoBlockCount * BytesPerBlock, '\0');
    receivedNamePayload_ = QByteArray(TextBlockCount * BytesPerBlock, '\0');
    receivedDeptPayload_ = QByteArray(TextBlockCount * BytesPerBlock, '\0');
    receivedPhotoBlocks_ = QVector<bool>(PhotoBlockCount, false);
    receivedNameBlocks_ = QVector<bool>(TextBlockCount, false);
    receivedDeptBlocks_ = QVector<bool>(TextBlockCount, false);
}

void MainWindow::updateImageFromBlocks()
{
    if (!receivedPhotoBlocks_.contains(false)) {
        photoBinaryPreview_ = unpackMonoImage(receivedPhotoPayload_, PhotoWidth, PhotoHeight);
        photoPreview_->setPixmap(QPixmap::fromImage(photoBinaryPreview_).scaled(
            photoPreview_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    if (!receivedNameBlocks_.contains(false)) {
        namePreview_ = unpackMonoImage(receivedNamePayload_, TextWidth, TextHeight);
        namePreviewLabel_->setPixmap(QPixmap::fromImage(namePreview_).scaled(
            namePreviewLabel_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    if (!receivedDeptBlocks_.contains(false)) {
        deptPreview_ = unpackMonoImage(receivedDeptPayload_, TextWidth, TextHeight);
        deptPreviewLabel_->setPixmap(QPixmap::fromImage(deptPreview_).scaled(
            deptPreviewLabel_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
}

QImage MainWindow::unpackMonoImage(const QByteArray &payload, int width, int height) const
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::white);

    int byteIndex = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            const uchar byte = static_cast<uchar>(payload.at(byteIndex++));
            for (int bit = 0; bit < 8; ++bit) {
                const bool black = (byte & (1 << (7 - bit))) != 0;
                image.setPixel(x + bit, y, black ? qRgb(0, 0, 0) : qRgb(255, 255, 255));
            }
        }
    }

    return image;
}

int MainWindow::selectedCardType() const
{
    return cardTypeCombo_->currentIndex();
}

QString MainWindow::selectedPortName() const
{
    return portCombo_->currentData().toString();
}

QString MainWindow::typeText(int type) const
{
    switch (type) {
    case 0:
        return "普通卡";
    case 1:
        return "图像卡";
    case 2:
        return "管理员卡";
    default:
        return "未知";
    }
}

// ═══════════════════════════════════════════════════
// 记录查询
// ═══════════════════════════════════════════════════

void MainWindow::searchRecords()
{
    // 如果串口已连接，先从设备拉取全部记录，再本地筛选
    if (serial_.isOpen()) {
        pendingDeviceSyncForSearch_ = true;
        attendanceTable_->setRowCount(0);   // 立即清空旧结果
        logMessage("正在从设备获取全部考勤记录...");
        sendLine("LIST:ALL");
        return;
    }

    // 串口未连接，直接搜索本地数据库
    performLocalSearch();
}

void MainWindow::performLocalSearch()
{
    RecordFilter filter;
    filter.startTime  = searchStartTime_->dateTime().toString("yyyy-MM-dd HH:mm:ss");
    filter.endTime    = searchEndTime_->dateTime().toString("yyyy-MM-dd HH:mm:ss");
    filter.sid        = searchSidEdit_->text().trimmed();
    filter.name       = searchNameEdit_->text().trimmed();
    filter.department = searchDeptEdit_->text().trimmed();

    DatabaseManager &db = DatabaseManager::instance();
    if (!db.isOpen()) {
        QMessageBox::warning(this, "提示", "数据库未就绪，无法查询。");
        return;
    }

    const QList<AttendanceRecord> records = db.searchRecords(filter);
    displaySearchResults(records);
    logMessage(QString("查询完成，共 %1 条记录").arg(records.size()));
}

void MainWindow::clearSearchFilter()
{
    searchStartTime_->setDateTime(QDateTime::currentDateTime().addDays(-7));
    searchEndTime_->setDateTime(QDateTime::currentDateTime());
    searchSidEdit_->clear();
    searchNameEdit_->clear();
    searchDeptEdit_->clear();
    searchResultLabel_->clear();
}

void MainWindow::displaySearchResults(const QList<AttendanceRecord> &records)
{
    attendanceTable_->setRowCount(0);

    // 查询结果仅显示：打卡时间、学号、姓名、部门、状态
    attendanceTable_->setColumnCount(5);
    attendanceTable_->setHorizontalHeaderLabels({"打卡时间", "学号", "姓名", "部门", "状态"});

    for (const AttendanceRecord &rec : records) {
        // 格式化时间：使用设备原始时间 rawTime
        QString formattedTime = rec.rawTime;
        const QDateTime dt = QDateTime::fromString(rec.rawTime, "yyyy-MM-dd HH:mm:ss");
        if (dt.isValid()) {
            formattedTime = dt.toString("yyyy年MM月dd日 HH:mm:ss");
        }

        // 格式化状态
        QString formattedStatus = rec.status;
        if (rec.status == "IN")           formattedStatus = "签到";
        else if (rec.status == "OUT")     formattedStatus = "离开";
        else if (rec.status == "DUP")     formattedStatus = "重复刷卡";
        else if (rec.status == "NOIN")    formattedStatus = "无入场离开";
        else if (rec.status == "INV")     formattedStatus = "无效卡";

        // 检查卡状态（挂失/注销）
        IssueRecord issueRec;
        if (DatabaseManager::instance().fetchIssueRecord(rec.uid, issueRec)) {
            if (issueRec.status == "lost") {
                formattedStatus += " [挂失卡]";
            } else if (issueRec.status == "cancelled") {
                formattedStatus += " [已注销]";
            }
        }

        // 从发卡信息库按学号补全姓名/部门（实时查询，确保最新）
        QString displayName = rec.name;
        QString displayDept = rec.department;
        if (!rec.sid.isEmpty()) {
            DatabaseManager::instance().lookupNameDeptBySid(rec.sid, displayName, displayDept);
        }
        // 若按学号未查到，再按卡号查
        if (displayName.isEmpty() && displayDept.isEmpty() && !rec.uid.isEmpty()) {
            DatabaseManager::instance().lookupNameDept(rec.uid, displayName, displayDept);
        }

        const int row = attendanceTable_->rowCount();
        attendanceTable_->insertRow(row);
        attendanceTable_->setItem(row, 0, cell(formattedTime));
        attendanceTable_->setItem(row, 1, cell(rec.sid));
        attendanceTable_->setItem(row, 2, cell(displayName));
        attendanceTable_->setItem(row, 3, cell(displayDept));
        attendanceTable_->setItem(row, 4, cell(formattedStatus));
    }

    searchResultLabel_->setText(QString("共查询到 %1 条记录").arg(records.size()));

    // 让列宽自适应内容
    attendanceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

// ═══════════════════════════════════════════════════
// 人员管理
// ═══════════════════════════════════════════════════

void MainWindow::refreshPersonnelTable()
{
    DatabaseManager &db = DatabaseManager::instance();
    if (!db.isOpen()) return;

    const QList<IssueRecord> list = db.fetchAllIssueRecords();
    personnelTable_->setRowCount(0);
    personnelTable_->setColumnCount(7);
    personnelTable_->setHorizontalHeaderLabels({"卡号UID", "学号", "姓名", "部门", "卡类型", "积分", "状态"});

    for (const IssueRecord &rec : list) {
        const int row = personnelTable_->rowCount();
        personnelTable_->insertRow(row);
        personnelTable_->setItem(row, 0, cell(rec.uid));
        personnelTable_->setItem(row, 1, cell(rec.sid));
        personnelTable_->setItem(row, 2, cell(rec.name));
        personnelTable_->setItem(row, 3, cell(rec.department));
        personnelTable_->setItem(row, 4, cell(rec.cardType));
        personnelTable_->setItem(row, 5, cell(rec.points));

        // 将数据库主键 id 隐藏在首列 UserRole 中，用于后续精确操作
        personnelTable_->item(row, 0)->setData(Qt::UserRole, rec.id);

        // 状态中文显示
        QString statusText = rec.status;
        if (rec.status == "normal")      statusText = "正常";
        else if (rec.status == "lost")   statusText = "[挂失]";
        else if (rec.status == "cancelled") statusText = "[已注销]";
        QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
        if (rec.status == "lost" || rec.status == "cancelled") {
            statusItem->setForeground(Qt::red);
        }
        personnelTable_->setItem(row, 6, statusItem);
    }

    personnelTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::reportCardLost()
{
    const int row = personnelTable_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先在人员列表中选择一行。");
        return;
    }

    QTableWidgetItem *item = personnelTable_->item(row, 0);
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    const QString uid = item->text();
    DatabaseManager &db = DatabaseManager::instance();

    IssueRecord rec;
    if (!db.fetchIssueRecord(uid, rec)) {
        QMessageBox::warning(this, "提示", "未找到该人员记录。");
        return;
    }

    QString newStatus;
    QString actionText;
    if (rec.status == "lost") {
        // 解除挂失
        newStatus = "normal";
        actionText = "解除挂失";
    } else {
        // 挂失
        newStatus = "lost";
        actionText = "挂失";
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认操作",
        QString("确认要对 %1（UID=%2）执行【%3】操作吗？")
            .arg(rec.name.isEmpty() ? rec.sid : rec.name, uid, actionText),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QString error;
    if (db.setCardStatus(id, newStatus, &error)) {
        logMessage(QString("人员 %1: %2 成功").arg(rec.name.isEmpty() ? uid : rec.name, actionText));

        // ── 同步通知下位机 ──
        if (serial_.isOpen()) {
            if (newStatus == "lost") {
                // 下发 LOST 指令：通知下位机将 UID 加入黑名单
                sendLine(QString("LOST:%1").arg(uid));
                logMessage(QString("已向下位机发送 LOST 指令: UID=%1（下位机需固件支持）").arg(uid));
            } else {
                // 解除挂失：通知下位机从黑名单移除
                sendLine(QString("UNLOST:%1").arg(uid));
                logMessage(QString("已向下位机发送 UNLOST 指令: UID=%1（下位机需固件支持）").arg(uid));
            }
        }

        refreshPersonnelTable();
    } else {
        QMessageBox::critical(this, "操作失败", error);
    }
}

void MainWindow::replaceCard()
{
    const int row = personnelTable_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先在人员列表中选择要补卡的人员。");
        return;
    }

    QTableWidgetItem *item = personnelTable_->item(row, 0);
    if (!item) return;
    pendingReplaceOldId_ = item->data(Qt::UserRole).toInt();
    pendingReplaceOldUid_ = item->text();
    DatabaseManager &db = DatabaseManager::instance();

    if (!db.fetchIssueRecord(pendingReplaceOldUid_, pendingReplaceOldRec_)) {
        QMessageBox::warning(this, "提示", "未找到该人员记录。");
        return;
    }

    if (!serial_.isOpen()) {
        QMessageBox::warning(this, "提示", "请先打开串口。");
        return;
    }

    // 发送读卡指令，等待 UID 响应后由 completeReplaceCard 继续
    pendingReplaceCard_ = true;
    clearForm();
    sendLine("READ");
    logMessage(QString("补卡: 等待读取新卡UID (原卡=%1, 人员=%2)")
        .arg(pendingReplaceOldUid_, pendingReplaceOldRec_.name));
}

void MainWindow::completeReplaceCard(const QString &newUid)
{
    const QString newUidUpper = newUid.toUpper();
    DatabaseManager &db = DatabaseManager::instance();

    // 检查新卡号是否已被其他人使用
    IssueRecord existRec;
    if (db.fetchIssueRecord(newUidUpper, existRec)) {
        QMessageBox::warning(this, "提示",
            QString("新卡号 %1 已被 %2（学号 %3）使用，请更换卡片。")
                .arg(newUidUpper, existRec.name, existRec.sid));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认补卡",
        QString("确认补卡操作？\n\n人员: %1\n旧卡: %2 → 新卡: %3\n\n旧卡将被标记为「挂失」状态。")
            .arg(pendingReplaceOldRec_.name, pendingReplaceOldUid_, newUidUpper),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // 1. 将旧卡标记为 lost，并释放原学号给新卡
    QString error;
    db.setCardStatus(pendingReplaceOldId_, "lost", &error);

    // 保存原始学号，旧卡学号加后缀以释放 UNIQUE 约束
    const QString originalSid = pendingReplaceOldRec_.sid;
    pendingReplaceOldRec_.id = pendingReplaceOldId_;
    pendingReplaceOldRec_.sid = originalSid + "_挂失";
    pendingReplaceOldRec_.status = "lost";
    db.updateIssueRecord(pendingReplaceOldRec_, &error);

    // 通知下位机将旧卡加入黑名单
    if (serial_.isOpen()) {
        sendLine(QString("LOST:%1").arg(pendingReplaceOldUid_));
        logMessage(QString("已向下位机发送 LOST 指令: UID=%1").arg(pendingReplaceOldUid_));
    }

    // 2. 创建新记录（使用原始学号）
    IssueRecord newRec = pendingReplaceOldRec_;
    newRec.uid = newUidUpper;
    newRec.sid = originalSid;
    newRec.status = "normal";

    if (db.insertOrUpdateIssueRecord(newRec, &error)) {
        // 3. 复制旧卡的图像文件到新卡
        const QString oldBase = issueImageDir() + "/" + pendingReplaceOldUid_;
        const QString newBase = issueImageDir() + "/" + newUidUpper;
        if (QFile::exists(oldBase + "_photo.png"))
            QFile::copy(oldBase + "_photo.png", newBase + "_photo.png");
        if (QFile::exists(oldBase + "_name.png"))
            QFile::copy(oldBase + "_name.png", newBase + "_name.png");
        if (QFile::exists(oldBase + "_dept.png"))
            QFile::copy(oldBase + "_dept.png", newBase + "_dept.png");

        logMessage(QString("补卡成功: %1 UID %2 → %3")
            .arg(pendingReplaceOldRec_.name, pendingReplaceOldUid_, newUidUpper));

        // 自动填充发卡表单
        cardEdit_->setText(newUidUpper);
        studentEdit_->setText(originalSid);
        pointsEdit_->setText(pendingReplaceOldRec_.points);
        nameEdit_->setText(pendingReplaceOldRec_.name);
        deptEdit_->setText(pendingReplaceOldRec_.department);
        const int typeIndex = cardTypeCombo_->findText(pendingReplaceOldRec_.cardType);
        if (typeIndex >= 0) cardTypeCombo_->setCurrentIndex(typeIndex);
        loadIssueRecord(newUidUpper);

        // 图像卡：恢复 selectedPhoto_
        if (typeIndex == 1) {
            QImage savedPhoto(issueImageDir() + "/" + newUidUpper + "_photo.png");
            if (!savedPhoto.isNull()) {
                selectedPhoto_ = savedPhoto;
            } else {
                QMessageBox::warning(this, "提示",
                    QString("数据库已更新，但未找到头像图片。\n\n请先点击「选择头像图片」加载头像，再点击「发卡写卡」。"));
                refreshPersonnelTable();
                return;
            }
        }

        // 串口检查
        if (!serial_.isOpen()) {
            QMessageBox::warning(this, "提示",
                QString("数据库已更新。\n\n串口未打开，未能写入新卡。请打开串口后点击「发卡写卡」完成写卡。"));
            refreshPersonnelTable();
            return;
        }

        refreshPersonnelTable();

        // 直接走发卡写卡流程
        writeCard();
    } else {
        QMessageBox::critical(this, "补卡失败", error);
    }
}

void MainWindow::modifyPersonnel()
{
    const int row = personnelTable_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先在人员列表中选择要修改的人员。");
        return;
    }

    QTableWidgetItem *item = personnelTable_->item(row, 0);
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    const QString uid = item->text();
    DatabaseManager &db = DatabaseManager::instance();

    IssueRecord rec;
    if (!db.fetchIssueRecord(uid, rec)) {
        QMessageBox::warning(this, "提示", "未找到该人员记录。");
        return;
    }

    // 弹出修改对话框
    QDialog dialog(this);
    dialog.setWindowTitle("修改人员信息 — " + rec.name);
    dialog.setMinimumWidth(400);

    auto *layout = new QFormLayout(&dialog);
    auto *nameEdit = new QLineEdit(rec.name, &dialog);
    auto *deptEdit = new QLineEdit(rec.department, &dialog);
    auto *sidEdit = new QLineEdit(rec.sid, &dialog);
    auto *pointsEdit = new QLineEdit(rec.points, &dialog);

    layout->addRow("姓名:", nameEdit);
    layout->addRow("部门:", deptEdit);
    layout->addRow("学号:", sidEdit);
    layout->addRow("积分:", pointsEdit);

    auto *btnLayout = new QHBoxLayout();
    auto *okBtn = new QPushButton("确认修改", &dialog);
    auto *cancelBtn = new QPushButton("取消", &dialog);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addRow(btnLayout);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;

    const QString newName = nameEdit->text().trimmed();
    const QString newDept = deptEdit->text().trimmed();
    const QString newSid = sidEdit->text().trimmed();

    if (newName.isEmpty() || newDept.isEmpty() || newSid.isEmpty()) {
        QMessageBox::warning(this, "提示", "姓名、部门、学号不能为空。");
        return;
    }

    if (!isEightDigitStudentId(newSid)) {
        QMessageBox::warning(this, "提示", "学号必须是8位数字。");
        return;
    }

    // 检查新学号是否与其他记录冲突
    IssueRecord sidCheck;
    if (db.fetchIssueRecordBySid(newSid, sidCheck) && sidCheck.uid != uid) {
        QMessageBox::warning(this, "提示",
            QString("学号 %1 已被 %2 使用。").arg(newSid, sidCheck.name));
        return;
    }

    rec.name = newName;
    rec.department = newDept;
    rec.sid = newSid;
    rec.points = pointsEdit->text().trimmed();
    rec.id = id;

    // 图像卡：尝试加载已有图片，没有则在 writeCard 时提示
    const int tIdx = cardTypeCombo_->findText(rec.cardType);
    if (tIdx == 1) {
        QImage savedPhoto(issueImageDir() + "/" + uid + "_photo.png");
        if (!savedPhoto.isNull()) {
            selectedPhoto_ = savedPhoto;
        }
        // 没有图片也不阻断，writeCard 会提示用户选择
    }

    if (!serial_.isOpen()) {
        QMessageBox::warning(this, "提示", "请先打开串口。");
        return;
    }

    QString error;
    if (db.updateIssueRecord(rec, &error)) {
        logMessage(QString("人员信息已更新: %1 (UID=%2)").arg(rec.name, uid));
        refreshPersonnelTable();

        cardEdit_->setText(uid);
        studentEdit_->setText(newSid);
        pointsEdit_->setText(rec.points);
        nameEdit_->setText(newName);
        deptEdit_->setText(newDept);
        if (tIdx >= 0) cardTypeCombo_->setCurrentIndex(tIdx);
        loadIssueRecord(uid);

        writeCard();
    } else {
        QMessageBox::critical(this, "修改失败", error);
    }
}

void MainWindow::deletePersonnel()
{
    const int row = personnelTable_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先在人员列表中选择要删除的人员。");
        return;
    }

    QTableWidgetItem *item = personnelTable_->item(row, 0);
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    const QString uid = item->text();
    DatabaseManager &db = DatabaseManager::instance();

    IssueRecord rec;
    if (!db.fetchIssueRecord(uid, rec)) {
        QMessageBox::warning(this, "提示", "未找到该人员记录。");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除",
        QString("确认要删除以下人员吗？\n\n姓名: %1\n学号: %2\nUID: %3\n\n此操作不可恢复，考勤记录不受影响。")
            .arg(rec.name, rec.sid, uid),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QString error;
    if (db.deleteIssueRecord(id, &error)) {
        logMessage(QString("人员已删除: %1 (UID=%2)").arg(rec.name, uid));
        syncIssueRecordCsv();
        refreshPersonnelTable();

        // 询问是否需要同时清除物理卡片
        QMessageBox::StandardButton clearReply = QMessageBox::question(
            this, "清除物理卡片",
            QString("数据库记录已删除。\n\n是否同时清除物理卡片 %1 上的数据？\n\n请先将该卡放在读卡器上。").arg(uid),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (clearReply == QMessageBox::Yes) {
            cardEdit_->setText(uid);
            if (serial_.isOpen()) {
                sendLine("CLEAR:" + uid);
                logMessage(QString("已发送清卡指令: CLEAR:%1").arg(uid));
            } else {
                QMessageBox::warning(this, "提示",
                    "串口未打开，无法清除物理卡片。\n\n请打开串口后，手动输入 UID 并点击「清卡」。");
            }
        }
    } else {
        QMessageBox::critical(this, "删除失败", error);
    }
}

void MainWindow::syncIssueRecordCsv()
{
    QFile file(issueRecordPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(true);
#else
    out.setEncoding(QStringConverter::Utf8);
    file.write("\xEF\xBB\xBF");
#endif
    out << "time,cid,student_id,points,type,name,department,status\n";

    const QList<IssueRecord> all = DatabaseManager::instance().fetchAllIssueRecords();
    for (const IssueRecord &r : all) {
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << ','
            << r.uid << ',' << r.sid << ',' << r.points << ','
            << r.cardType << ',' << r.name << ',' << r.department << ',' << r.status << '\n';
    }
}

// ═══════════════════════════════════════════════════
// 心跳 / 设备状态
// ═══════════════════════════════════════════════════

void MainWindow::handleHeartbeat(const QString &line)
{
    // 实际格式: HEART:DEV=1|MODE=2|TEMP=25.5|WIFI=1|PEND=3
    const QString body = line.mid(6);
    const QStringList fields = body.split('|');

    QString dev, mode, temp, wifi, pend;
    for (const QString &field : fields) {
        if (field.startsWith("DEV="))       dev  = field.mid(4);
        else if (field.startsWith("MODE=")) mode = field.mid(5);
        else if (field.startsWith("TEMP=")) temp = field.mid(5);
        else if (field.startsWith("WIFI=")) wifi = field.mid(5);
        else if (field.startsWith("PEND=")) pend = field.mid(5);
    }

    updateDeviceStatusDisplay(dev, mode, temp, wifi, pend);

    // 复位超时定时器
    heartbeatTimer_->start(HeartbeatTimeoutMs);
}

void MainWindow::onHeartbeatTimeout()
{
    // 8秒内未收到心跳，标记为离线
    heartbeatStatusLabel_->setText("● 离线");
    heartbeatStatusLabel_->setStyleSheet("color: red; font-weight: bold;");
    devIdLabel_->setText("--");
    devModeLabel_->setText("--");
    devTempLabel_->setText("--");
    devWifiLabel_->setText("--");
    devPendLabel_->setText("--");
    logMessage("⚠ 设备心跳超时，已离线");
}

void MainWindow::updateDeviceStatusDisplay(const QString &dev, const QString &mode,
                                            const QString &temp, const QString &wifi,
                                            const QString &pend)
{
    heartbeatStatusLabel_->setText("● 在线");
    heartbeatStatusLabel_->setStyleSheet("color: green; font-weight: bold;");
    devIdLabel_->setText(dev.isEmpty() ? "--" : QString("编号%1").arg(dev));
    // MODE: 1=考勤模式, 2=发卡模式（按需扩展）
    if (mode == "1")      devModeLabel_->setText("考勤模式");
    else if (mode == "2") devModeLabel_->setText("发卡模式");
    else                   devModeLabel_->setText(mode.isEmpty() ? "--" : mode);
    devTempLabel_->setText(temp.isEmpty() ? "--" : temp + " °C");
    // WIFI: 1=已连接, 0=未连接
    if (wifi == "1")      devWifiLabel_->setText("已连接");
    else if (wifi == "0") devWifiLabel_->setText("未连接");
    else                   devWifiLabel_->setText(wifi.isEmpty() ? "--" : wifi);
    devPendLabel_->setText(pend.isEmpty() ? "--" : pend + " 条");
}
