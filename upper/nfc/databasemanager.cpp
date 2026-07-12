#include "databasemanager.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#include <QDebug>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString &dbPath)
{
    if (db_.isOpen()) {
        return true;
    }

    const QString path = dbPath.isEmpty()
        ? QCoreApplication::applicationDirPath() + "/attendance.db"
        : dbPath;

    // 避免重复添加数据库连接（重复调用 addDatabase 会导致旧连接失效）
    const QString connName = "attendance_conn";
    if (QSqlDatabase::contains(connName)) {
        db_ = QSqlDatabase::database(connName);
    } else {
        db_ = QSqlDatabase::addDatabase("QSQLITE", connName);
    }
    db_.setDatabaseName(path);

    if (!db_.open()) {
        qWarning() << "数据库打开失败:" << db_.lastError().text();
        return false;
    }

    // 启用 WAL 模式提升并发写入性能
    QSqlQuery pragma(db_);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA synchronous=NORMAL");

    return createTable();
}

void DatabaseManager::close()
{
    if (db_.isOpen()) {
        db_.close();
    }
}

bool DatabaseManager::isOpen() const
{
    return db_.isOpen();
}

bool DatabaseManager::createTable()
{
    QSqlQuery query(db_);
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS attendance_records ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  seq             TEXT,"
        "  uid             TEXT    NOT NULL,"
        "  sid             TEXT,"
        "  card_type       TEXT,"
        "  raw_time        TEXT    NOT NULL,"
        "  calibrated_time TEXT    NOT NULL,"
        "  device          TEXT,"
        "  status          TEXT,"
        "  created_at      TEXT    DEFAULT (datetime('now','localtime'))"
        ")");

    if (!query.exec(sql)) {
        qWarning() << "建表失败:" << query.lastError().text();
        return false;
    }

    // 兼容旧表：添加姓名、部门列（若不存在则新增）
    query.exec("ALTER TABLE attendance_records ADD COLUMN name TEXT");
    query.exec("ALTER TABLE attendance_records ADD COLUMN department TEXT");

    // 为常用查询字段建立索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_uid ON attendance_records(uid)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_sid ON attendance_records(sid)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_calibrated_time ON attendance_records(calibrated_time)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_raw_time ON attendance_records(raw_time)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_name ON attendance_records(name)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_department ON attendance_records(department)");

    // ── 发卡记录表（本地信息库）──
    query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS issue_records ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  uid         TEXT    NOT NULL,"
        "  sid         TEXT    NOT NULL UNIQUE,"
        "  points      TEXT    DEFAULT '0',"
        "  name        TEXT,"
        "  department  TEXT,"
        "  card_type   TEXT,"
        "  status      TEXT    DEFAULT 'normal'"
        ")"));
    query.exec("ALTER TABLE issue_records ADD COLUMN status TEXT DEFAULT 'normal'");
    query.exec("CREATE INDEX IF NOT EXISTS idx_issue_uid ON issue_records(uid)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_issue_sid ON issue_records(sid)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_issue_status ON issue_records(status)");

    // 从旧 CSV 迁移数据到数据库（仅当 issue_records 为空时执行一次）
    {
        QSqlQuery countQuery(db_);
        countQuery.exec("SELECT COUNT(*) FROM issue_records");
        if (countQuery.next() && countQuery.value(0).toInt() == 0) {
            const QString csvPath = QCoreApplication::applicationDirPath() + "/issue_records.csv";
            QFile csvFile(csvPath);
            if (csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&csvFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                in.setCodec("UTF-8");
#else
                in.setEncoding(QStringConverter::Utf8);
#endif
                bool firstLine = true;
                while (!in.atEnd()) {
                    const QString line = in.readLine().trimmed();
                    if (line.isEmpty()) continue;
                    if (firstLine) { firstLine = false; continue; } // 跳过表头
                    const QStringList f = line.split(',');
                    if (f.size() >= 7) {
                        QSqlQuery ins(db_);
                        ins.prepare("INSERT OR IGNORE INTO issue_records (uid, sid, points, card_type, name, department) VALUES (?, ?, ?, ?, ?, ?)");
                        ins.addBindValue(f.value(1).trimmed());
                        ins.addBindValue(f.value(2).trimmed());
                        ins.addBindValue(f.value(3).trimmed());
                        ins.addBindValue(f.value(4).trimmed());
                        ins.addBindValue(f.value(5).trimmed());
                        ins.addBindValue(f.value(6).trimmed());
                        ins.exec();
                    }
                }
            }
        }
    }

    return true;
}

bool DatabaseManager::insertAttendanceRecord(const AttendanceRecord &record, QString *errorOut)
{
    if (!db_.isOpen()) {
        const QString err = "数据库未打开，无法插入记录";
        qWarning() << err;
        if (errorOut) *errorOut = err;
        return false;
    }

    // 尝试从发卡记录中查找姓名和部门（先按卡号，再按学号）
    QString name = record.name;
    QString department = record.department;
    if (name.isEmpty() && department.isEmpty()) {
        lookupNameDept(record.uid, name, department);
        if (name.isEmpty() && department.isEmpty()) {
            lookupNameDeptBySid(record.sid, name, department);
        }
    }

    // 去重：同一卡号+同一原始时间视为重复记录，跳过
    {
        QSqlQuery dup(db_);
        dup.prepare("SELECT COUNT(*) FROM attendance_records WHERE uid = ? AND raw_time = ?");
        dup.addBindValue(record.uid);
        dup.addBindValue(record.rawTime);
        if (dup.exec() && dup.next() && dup.value(0).toInt() > 0) {
            return true; // 已存在，静默跳过
        }
    }

    QSqlQuery query(db_);
    // 使用位置占位符 ? 代替命名参数，避免 Qt SQLite 驱动的 "Parameter count mismatch" 问题
    query.prepare(QStringLiteral(
        "INSERT INTO attendance_records "
        "(seq, uid, sid, card_type, raw_time, calibrated_time, device, status, name, department) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    query.addBindValue(record.seq);
    query.addBindValue(record.uid);
    query.addBindValue(record.sid);
    query.addBindValue(record.cardType);
    query.addBindValue(record.rawTime);
    query.addBindValue(record.calibratedTime);
    query.addBindValue(record.device);
    query.addBindValue(record.status);
    query.addBindValue(name);
    query.addBindValue(department);

    if (!query.exec()) {
        const QString err = query.lastError().text();
        qWarning() << "插入考勤记录失败:" << err;
        if (errorOut) *errorOut = err;
        return false;
    }

    return true;
}

QList<AttendanceRecord> DatabaseManager::fetchAllRecords(int limit) const
{
    QList<AttendanceRecord> records;

    if (!db_.isOpen()) {
        return records;
    }

    QSqlQuery query(db_);
    QString sql = "SELECT seq, uid, sid, card_type, raw_time, calibrated_time, device, status, name, department "
                  "FROM attendance_records ORDER BY raw_time DESC";
    if (limit > 0) {
        sql += " LIMIT " + QString::number(limit);
    }

    if (!query.exec(sql)) {
        qWarning() << "查询考勤记录失败:" << query.lastError().text();
        return records;
    }

    while (query.next()) {
        AttendanceRecord rec;
        rec.seq            = query.value(0).toString();
        rec.uid            = query.value(1).toString();
        rec.sid            = query.value(2).toString();
        rec.cardType       = query.value(3).toString();
        rec.rawTime        = query.value(4).toString();
        rec.calibratedTime = query.value(5).toString();
        rec.device         = query.value(6).toString();
        rec.status         = query.value(7).toString();
        rec.name           = query.value(8).toString();
        rec.department     = query.value(9).toString();
        records.append(rec);
    }

    return records;
}

QList<AttendanceRecord> DatabaseManager::fetchRecordsByUid(const QString &uid) const
{
    QList<AttendanceRecord> records;

    if (!db_.isOpen()) {
        return records;
    }

    QSqlQuery query(db_);
    query.prepare("SELECT seq, uid, sid, card_type, raw_time, calibrated_time, device, status, name, department "
                  "FROM attendance_records WHERE uid = ? ORDER BY id DESC");
    query.addBindValue(uid);

    if (!query.exec()) {
        qWarning() << "按UID查询失败:" << query.lastError().text();
        return records;
    }

    while (query.next()) {
        AttendanceRecord rec;
        rec.seq            = query.value(0).toString();
        rec.uid            = query.value(1).toString();
        rec.sid            = query.value(2).toString();
        rec.cardType       = query.value(3).toString();
        rec.rawTime        = query.value(4).toString();
        rec.calibratedTime = query.value(5).toString();
        rec.device         = query.value(6).toString();
        rec.status         = query.value(7).toString();
        rec.name           = query.value(8).toString();
        rec.department     = query.value(9).toString();
        records.append(rec);
    }

    return records;
}

int DatabaseManager::recordCount() const
{
    if (!db_.isOpen()) {
        return 0;
    }

    QSqlQuery query(db_);
    if (!query.exec("SELECT COUNT(*) FROM attendance_records")) {
        return 0;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool DatabaseManager::clearAllRecords()
{
    if (!db_.isOpen()) {
        return false;
    }

    QSqlQuery query(db_);
    if (!query.exec("DELETE FROM attendance_records")) {
        qWarning() << "清空考勤记录失败:" << query.lastError().text();
        return false;
    }

    return true;
}

QList<AttendanceRecord> DatabaseManager::searchRecords(const RecordFilter &filter) const
{
    QList<AttendanceRecord> records;

    if (!db_.isOpen()) {
        return records;
    }

    QString sql = "SELECT seq, uid, sid, card_type, raw_time, calibrated_time, device, status, name, department "
                  "FROM attendance_records WHERE 1=1";
    QStringList conditions;
    QVariantList bindValues;

    // 时间范围：使用校准时间字段
    if (!filter.startTime.isEmpty()) {
        conditions.append("calibrated_time >= ?");
        bindValues.append(filter.startTime);
    }
    if (!filter.endTime.isEmpty()) {
        conditions.append("calibrated_time <= ?");
        bindValues.append(filter.endTime);
    }

    // 学号（模糊匹配）
    if (!filter.sid.isEmpty()) {
        conditions.append("sid LIKE ?");
        bindValues.append("%" + filter.sid + "%");
    }

    // 姓名（模糊匹配）
    if (!filter.name.isEmpty()) {
        conditions.append("name LIKE ?");
        bindValues.append("%" + filter.name + "%");
    }

    // 部门（模糊匹配）
    if (!filter.department.isEmpty()) {
        conditions.append("department LIKE ?");
        bindValues.append("%" + filter.department + "%");
    }

    // 拼接条件
    for (const QString &cond : conditions) {
        sql += " AND " + cond;
    }

    sql += " ORDER BY raw_time DESC";

    if (filter.limit > 0) {
        sql += " LIMIT " + QString::number(filter.limit);
    }

    QSqlQuery query(db_);
    query.prepare(sql);

    for (int i = 0; i < bindValues.size(); ++i) {
        query.bindValue(i, bindValues.at(i));
    }

    if (!query.exec()) {
        qWarning() << "多条件查询失败:" << query.lastError().text();
        return records;
    }

    while (query.next()) {
        AttendanceRecord rec;
        rec.seq            = query.value(0).toString();
        rec.uid            = query.value(1).toString();
        rec.sid            = query.value(2).toString();
        rec.cardType       = query.value(3).toString();
        rec.rawTime        = query.value(4).toString();
        rec.calibratedTime = query.value(5).toString();
        rec.device         = query.value(6).toString();
        rec.status         = query.value(7).toString();
        rec.name           = query.value(8).toString();
        rec.department     = query.value(9).toString();
        records.append(rec);
    }

    return records;
}

// ═══════════════════════════════════════════════════
// 发卡记录（本地信息库）
// ═══════════════════════════════════════════════════

bool DatabaseManager::insertOrUpdateIssueRecord(const IssueRecord &record, QString *errorOut)
{
    if (!db_.isOpen()) {
        if (errorOut) *errorOut = "数据库未打开";
        return false;
    }

    // 若已有相同学号的记录，先删除（学号相同 = 覆盖旧卡）
    QSqlQuery del(db_);
    del.prepare("DELETE FROM issue_records WHERE sid = ?");
    del.addBindValue(record.sid);
    if (!del.exec()) {
        if (errorOut) *errorOut = "删除旧记录失败: " + del.lastError().text();
        return false;
    }

    // 插入新记录（DELETE 已确保无 sid 冲突）
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "INSERT INTO issue_records (uid, sid, points, name, department, card_type, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));

    query.addBindValue(record.uid);
    query.addBindValue(record.sid);
    query.addBindValue(record.points);
    query.addBindValue(record.name);
    query.addBindValue(record.department);
    query.addBindValue(record.cardType);
    query.addBindValue(record.status.isEmpty() ? "normal" : record.status);

    if (!query.exec()) {
        const QString err = query.lastError().text();
        qWarning() << "插入发卡记录失败:" << err;
        if (errorOut) *errorOut = err;
        return false;
    }
    return true;
}

QList<IssueRecord> DatabaseManager::fetchAllIssueRecords() const
{
    QList<IssueRecord> list;
    if (!db_.isOpen()) return list;

    QSqlQuery query(db_);
    query.exec("SELECT id, uid, sid, points, name, department, card_type, status FROM issue_records ORDER BY id");
    while (query.next()) {
        IssueRecord rec;
        rec.id         = query.value(0).toInt();
        rec.uid        = query.value(1).toString();
        rec.sid        = query.value(2).toString();
        rec.points     = query.value(3).toString();
        rec.name       = query.value(4).toString();
        rec.department = query.value(5).toString();
        rec.cardType   = query.value(6).toString();
        rec.status     = query.value(7).toString();
        list.append(rec);
    }
    return list;
}

bool DatabaseManager::fetchIssueRecord(const QString &uid, IssueRecord &record) const
{
    if (!db_.isOpen() || uid.isEmpty()) return false;

    QSqlQuery query(db_);
    query.prepare("SELECT id, uid, sid, points, name, department, card_type, status "
                  "FROM issue_records WHERE uid = ?");
    query.addBindValue(uid);

    if (!query.exec() || !query.next()) return false;

    record.id         = query.value(0).toInt();
    record.uid        = query.value(1).toString();
    record.sid        = query.value(2).toString();
    record.points     = query.value(3).toString();
    record.name       = query.value(4).toString();
    record.department = query.value(5).toString();
    record.cardType   = query.value(6).toString();
    record.status     = query.value(7).toString();
    return true;
}

void DatabaseManager::lookupNameDept(const QString &uid, QString &name, QString &department) const
{
    if (uid.isEmpty()) return;

    QSqlQuery query(db_);
    query.prepare("SELECT name, department FROM issue_records WHERE uid = ?");
    query.addBindValue(uid);

    if (query.exec() && query.next()) {
        name       = query.value(0).toString();
        department = query.value(1).toString();
    }
}

void DatabaseManager::lookupNameDeptBySid(const QString &sid, QString &name, QString &department) const
{
    if (sid.isEmpty()) return;

    QSqlQuery query(db_);
    query.prepare("SELECT name, department FROM issue_records WHERE sid = ?");
    query.addBindValue(sid);

    if (query.exec() && query.next()) {
        name       = query.value(0).toString();
        department = query.value(1).toString();
    }
}

// ═══════════════════════════════════════════════════
// 人员管理新增方法
// ═══════════════════════════════════════════════════

bool DatabaseManager::updateIssueRecord(const IssueRecord &record, QString *errorOut)
{
    if (!db_.isOpen()) {
        if (errorOut) *errorOut = "数据库未打开";
        return false;
    }

    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "UPDATE issue_records SET sid=?, points=?, name=?, department=?, card_type=?, status=? "
        "WHERE id=?"));

    query.addBindValue(record.sid);
    query.addBindValue(record.points);
    query.addBindValue(record.name);
    query.addBindValue(record.department);
    query.addBindValue(record.cardType);
    query.addBindValue(record.status);
    query.addBindValue(record.id);

    if (!query.exec()) {
        const QString err = query.lastError().text();
        qWarning() << "更新发卡记录失败:" << err;
        if (errorOut) *errorOut = err;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteIssueRecord(int id, QString *errorOut)
{
    if (!db_.isOpen()) {
        if (errorOut) *errorOut = "数据库未打开";
        return false;
    }

    QSqlQuery query(db_);
    query.prepare("DELETE FROM issue_records WHERE id = ?");
    query.addBindValue(id);

    if (!query.exec()) {
        const QString err = query.lastError().text();
        qWarning() << "删除发卡记录失败:" << err;
        if (errorOut) *errorOut = err;
        return false;
    }
    return true;
}

bool DatabaseManager::setCardStatus(int id, const QString &status, QString *errorOut)
{
    if (!db_.isOpen()) {
        if (errorOut) *errorOut = "数据库未打开";
        return false;
    }

    QSqlQuery query(db_);
    query.prepare("UPDATE issue_records SET status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(id);

    if (!query.exec()) {
        const QString err = query.lastError().text();
        qWarning() << "更新卡状态失败:" << err;
        if (errorOut) *errorOut = err;
        return false;
    }
    return true;
}

bool DatabaseManager::fetchIssueRecordBySid(const QString &sid, IssueRecord &record) const
{
    if (!db_.isOpen() || sid.isEmpty()) return false;

    QSqlQuery query(db_);
    query.prepare("SELECT id, uid, sid, points, name, department, card_type, status "
                  "FROM issue_records WHERE sid = ?");
    query.addBindValue(sid);

    if (!query.exec() || !query.next()) return false;

    record.id         = query.value(0).toInt();
    record.uid        = query.value(1).toString();
    record.sid        = query.value(2).toString();
    record.points     = query.value(3).toString();
    record.name       = query.value(4).toString();
    record.department = query.value(5).toString();
    record.cardType   = query.value(6).toString();
    record.status     = query.value(7).toString();
    return true;
}
