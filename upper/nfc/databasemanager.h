#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QStringList>
#include <QSqlDatabase>

struct AttendanceRecord {
    QString seq;
    QString uid;
    QString sid;
    QString cardType;
    QString rawTime;        // 下位机上报的原始时间
    QString calibratedTime; // 上位机校准后的统一时间
    QString device;
    QString status;
    QString name;           // 姓名（从发卡记录关联）
    QString department;     // 部门（从发卡记录关联）
};

struct RecordFilter {
    QString startTime;      // yyyy-MM-dd HH:mm:ss 起始
    QString endTime;        // yyyy-MM-dd HH:mm:ss 截止
    QString sid;            // 学号（模糊匹配）
    QString name;           // 姓名（模糊匹配）
    QString department;     // 部门（模糊匹配）
    int     limit = -1;     // 返回条数限制，-1 表示全部
};

struct IssueRecord {
    int    id = 0;       // 数据库主键，用于精确定位记录
    QString uid;        // 卡号
    QString sid;        // 学号
    QString points;     // 积分数
    QString name;       // 姓名
    QString department; // 部门
    QString cardType;   // 卡类型
    QString status;     // 状态: normal(正常) / lost(挂失) / cancelled(已注销)
};

class DatabaseManager
{
public:
    static DatabaseManager &instance();

    bool open(const QString &dbPath = QString());
    void close();
    bool isOpen() const;

    // 考勤记录
    bool insertAttendanceRecord(const AttendanceRecord &record, QString *errorOut = nullptr);
    QList<AttendanceRecord> fetchAllRecords(int limit = -1) const;
    QList<AttendanceRecord> fetchRecordsByUid(const QString &uid) const;
    QList<AttendanceRecord> searchRecords(const RecordFilter &filter) const;
    int recordCount() const;
    bool clearAllRecords();

    // 发卡记录（本地信息库）
    bool insertOrUpdateIssueRecord(const IssueRecord &record, QString *errorOut = nullptr);
    bool updateIssueRecord(const IssueRecord &record, QString *errorOut = nullptr);
    bool deleteIssueRecord(int id, QString *errorOut = nullptr);
    bool setCardStatus(int id, const QString &status, QString *errorOut = nullptr);
    bool fetchIssueRecord(const QString &uid, IssueRecord &record) const;
    bool fetchIssueRecordBySid(const QString &sid, IssueRecord &record) const;
    QList<IssueRecord> fetchAllIssueRecords() const;
    void lookupNameDept(const QString &uid, QString &name, QString &department) const;
    void lookupNameDeptBySid(const QString &sid, QString &name, QString &department) const;

private:
    DatabaseManager() = default;
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool createTable();

    QSqlDatabase db_;
};

#endif // DATABASEMANAGER_H
