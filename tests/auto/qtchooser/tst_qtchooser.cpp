/****************************************************************************
**
** Copyright (C) 2012 Intel Corporation.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt tool chooser of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>

#ifdef Q_OS_WIN
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>
#endif

#ifdef Q_OS_WIN
#  define LIST_SEP ";"
#  define EXE_SUFFIX ".exe"
#else
#  define LIST_SEP ":"
#  define EXE_SUFFIX ""
#endif

#define VERIFY_NORMAL_EXIT(proc) \
    if (!(proc)) return; \
    QCOMPARE((proc)->readAllStandardError().constData(), ""); \
    QCOMPARE((proc)->exitCode(), 0)

class tst_ToolChooser : public QObject
{
    Q_OBJECT

public:
    enum {
        Copy = 0x1,
        Symlink = 0x2,
        Environment = 0x4,
        CommandLine = 0x8
    };
    QProcessEnvironment testModeEnvironment;
    QString testData;
    QString toolPath;
    QString pathsWithDefault;
    QString tempFileName;
    QString tempFileBaseName;

    tst_ToolChooser();
    inline QProcess *execute(const QStringList &arguments)
    { return execute(arguments, testModeEnvironment); }
    inline QProcess *execute(const QStringList &arguments, const QProcessEnvironment &env)
    { return execute(toolPath, arguments, env); }
    QProcess *execute(const QString &program, const QStringList &arguments, const QProcessEnvironment &env);


public Q_SLOTS:
    void initTestCase();
    void cleanup();

private Q_SLOTS:
    void list();
    void selectTool_data();
    void selectTool();
    void selectQt_data();
    void selectQt();
    void defaultQt_data();
    void defaultQt();
    void passArgs_data();
    void passArgs();
    void install_data();
    void install();
    void install2();
};

tst_ToolChooser::tst_ToolChooser()
    : testModeEnvironment(QProcessEnvironment::systemEnvironment())
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    testData = QFINDTESTDATA("testdata");
#else
    testData = SRCDIR "testdata";
#endif
    pathsWithDefault = testData + "/config1" LIST_SEP +
            testData + "/config2";
    testModeEnvironment.insert("XDG_CONFIG_HOME", "/dev/null");
    testModeEnvironment.insert("XDG_CONFIG_DIRS", pathsWithDefault);
    testModeEnvironment.insert("QT_SELECT", "4.8");

    pathsWithDefault.prepend(testData + "/default" LIST_SEP);

    toolPath = QCoreApplication::applicationDirPath() + "/../../../src/qtchooser/test/qtchooser" EXE_SUFFIX;

    tempFileBaseName = "tool-" + QString::number(getpid()) + ".exe";
    tempFileName = QDir::currentPath() + "/" + tempFileBaseName;
}

QProcess *tst_ToolChooser::execute(const QString &program, const QStringList &arguments, const QProcessEnvironment &env)
{
    QProcess *proc = new QProcess;
    proc->setProcessEnvironment(env);
    proc->start(program, arguments, QIODevice::ReadOnly | QIODevice::Text);
    if (!proc->waitForFinished()) {
        QTest::qFail("Executing '" + program.toLocal8Bit()
                     + "' failed: " + proc->errorString().toLocal8Bit(),
                     __FILE__, __LINE__);
        delete proc;
        return 0;
    }
    return proc;
}

void tst_ToolChooser::initTestCase()
{
    QVERIFY(QFile::exists(toolPath));
    QVERIFY(!QFile::exists(tempFileName));
}

void tst_ToolChooser::cleanup()
{
    QFile::remove(tempFileName);
}

void tst_ToolChooser::list()
{
    QScopedPointer<QProcess> proc(execute(QStringList() << "-list-versions", testModeEnvironment));
    VERIFY_NORMAL_EXIT(proc);

    QStringList foundVersions;
    while (!proc->atEnd()) {
        QByteArray line = proc->readLine().trimmed();
        QVERIFY(!line.isEmpty());
        QVERIFY(!foundVersions.contains(line));
        foundVersions << line;
    }

    qDebug() << foundVersions;
    QVERIFY(foundVersions.contains("4.8"));
    QVERIFY(foundVersions.contains("5"));
}

void tst_ToolChooser::selectTool_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<QString>("expected");

    QString tempExpected = "/" + tempFileBaseName;
    QTest::newRow("copy") << int(Copy) << tempExpected;
#ifdef Q_OS_UNIX
    QTest::newRow("symlink") << int(Symlink) << tempExpected;
#endif
    QTest::newRow("env") << int(Environment) << "/environ";
    QTest::newRow("cmdline") << int(CommandLine) << "/cmdline";

    // argv[0] overrides everything:
    QTest::newRow("copy+env") << int(Copy | Environment) << tempExpected;
    QTest::newRow("copy+cmdline") << int(Copy | CommandLine) << tempExpected;
    QTest::newRow("copy+env+cmdline") << int(Copy | Environment | CommandLine) << tempExpected;

    // the environment overrides the command-line:
    QTest::newRow("env+cmdline") << int(Environment | CommandLine) << "/environ";
}

void tst_ToolChooser::selectTool()
{
    QFETCH(int, mode);
    QProcessEnvironment env = testModeEnvironment;
    QStringList args;

    QFile source(toolPath);
    if (mode & Symlink) {
#ifndef Q_OS_UNIX
        qFatal("Impossible, cannot happen, you've broken the test!!");
#else
        // even though QTemporaryFile has the file opened, we'll overwrite it with a symlink
        // on Unix, we're allowed to do that
        QVERIFY(source.link(tempFileName));
#endif
    } else if (mode & Copy) {
        QFile tmp(tempFileName);
        QVERIFY(tmp.open(QIODevice::ReadWrite));
        QVERIFY(source.open(QIODevice::ReadOnly));
        tmp.write(source.readAll());
        tmp.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
        source.close();
    }

    QString exe = toolPath;
    if (mode & (Copy | Symlink)) {
        QVERIFY(QFile::exists(tempFileName));
        exe = tempFileName;
    }

    if (mode & Environment)
        env.insert("QTCHOOSER_RUNTOOL", "environ");
    if (mode & CommandLine)
        args << "-run-tool=cmdline";

    QScopedPointer<QProcess> proc(execute(exe, args, env));
    VERIFY_NORMAL_EXIT(proc);

    QFETCH(QString, expected);
    QByteArray procstdout = proc->readAllStandardOutput().trimmed();
    QVERIFY2(procstdout.contains(expected.toLatin1()), procstdout);
}

void tst_ToolChooser::selectQt_data()
{
    QTest::addColumn<bool>("useEnv");
    QTest::addColumn<QString>("select");
    QTest::addColumn<QString>("expected");

    QTest::newRow("cmdline-4.8") << false << "4.8" << "correct-4.8";
    QTest::newRow("env-4.8") << true << "4.8" << "correct-4.8";
    QTest::newRow("cmdline-5") << false << "5" << "qt5";
    QTest::newRow("env-5") << true << "5" << "qt5";

    QTest::newRow("cmdline-invalid") << false << "invalid" << QString();
    QTest::newRow("env-invalid") << true << "invalid" << QString();
}

void tst_ToolChooser::selectQt()
{
    QFETCH(bool, useEnv);
    QFETCH(QString, select);
    QFETCH(QString, expected);

    QProcessEnvironment env = testModeEnvironment;
    QStringList args;

    env.remove("QT_SELECT");
    if (useEnv)
        env.insert("QT_SELECT", select);
    else
        args << "-qt=" + select;
    args << "-print-env";

    QScopedPointer<QProcess> proc(execute(args, env));
    QVERIFY(proc);
    if (expected.isEmpty()) {
        // it is supposed to fail
        QCOMPARE(proc->readAllStandardOutput().constData(), "");
        QVERIFY(proc->exitCode() != 0);
        QVERIFY(!proc->readAllStandardError().isEmpty());
    } else {
        QCOMPARE(proc->readAllStandardError().constData(), "");
        QCOMPARE(proc->exitCode(), 0);

        // The first line is QT_SELECT= again
        QByteArray line = proc->readLine().trimmed();
        QVERIFY2(line.startsWith("QT_SELECT="), line);

        // The second line is QTTOOLDIR=
        line = proc->readLine().trimmed();
        QVERIFY2(line.startsWith("QTTOOLDIR="), line);
        QVERIFY2(line.contains(expected.toLatin1()), line);
        QVERIFY2(line.endsWith("tooldir\""), line);

        // The third line is QTLIBDIR=
        line = proc->readLine().trimmed();
        QVERIFY2(line.startsWith("QTLIBDIR="), line);
        QVERIFY2(line.contains(expected.toLatin1()), line);
        QVERIFY2(line.endsWith("libdir\""), line);
    }
}

void tst_ToolChooser::defaultQt_data()
{
    QTest::addColumn<bool>("withDefault");
    QTest::addColumn<QString>("expected");

    QTest::newRow("no-default") << false << QString();
    QTest::newRow("with-default") << true << "default-qt";
}

void tst_ToolChooser::defaultQt()
{
    QFETCH(bool, withDefault);
    QFETCH(QString, expected);

    QProcessEnvironment env = testModeEnvironment;
    env.remove("QT_SELECT");
    if (withDefault)
        env.insert("XDG_CONFIG_DIRS", pathsWithDefault);

    QScopedPointer<QProcess> proc(execute(QStringList() << "-run-tool=qmake", env));
    QVERIFY(proc);
    if (withDefault) {
        QCOMPARE(proc->readAllStandardError().constData(), "");
        QCOMPARE(proc->exitCode(), 0);

        QByteArray procstdout = proc->readAllStandardOutput().trimmed();
        QVERIFY2(procstdout.contains(expected.toLatin1()), procstdout);
    } else {
        // no default, the tool fails
        QVERIFY(proc->exitCode() != 0);
        QVERIFY(!proc->readAllStandardError().isEmpty());
        QByteArray procstdout = proc->readAllStandardOutput().trimmed();
        QVERIFY2(procstdout.isEmpty(), procstdout.constData());
    }
}

void tst_ToolChooser::passArgs_data()
{
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("empty") << QStringList() << QStringList();
    QTest::newRow("drop1") << (QStringList() << "-qt5") << QStringList();
    QTest::newRow("drop2") << (QStringList() << "-qt5" << "-qt5") << QStringList();

    QTest::newRow("dash-dash") << (QStringList() << "--") << QStringList();
    QTest::newRow("dash-dash-qt5") << (QStringList() << "--" << "-qt5") << (QStringList() << "-qt5");
    QTest::newRow("dash-dash-dash-dash") << (QStringList() << "--" << "--") << (QStringList() << "--");

    QTest::newRow("unknown-opt") << (QStringList() << "-query") << (QStringList() << "-query");
    QTest::newRow("unknown-opt-qt5") << (QStringList() << "-query" << "-qt5") << (QStringList() << "-query" << "-qt5");
    QTest::newRow("non-opt") << (QStringList() << ".") << (QStringList() << ".");
    QTest::newRow("non-opt-qt5") << (QStringList() << "." << "-qt5") << (QStringList() << "." << "-qt5");

    // Since we're running a tool with QTCHOOSER_RUNTOOL set, it should not
    // swallow the -print-env, -list-versions and -run-tool arguments either.
    QTest::newRow("list-versions") << (QStringList() << "-list-versions") << (QStringList() << "-list-versions");
    QTest::newRow("qt5-list-versions") << (QStringList() << "-qt5" << "-list-versions") << (QStringList() << "-list-versions");
    QTest::newRow("print-env") << (QStringList() << "-print-env") << (QStringList() << "-print-env");
    QTest::newRow("qt5-print-env") << (QStringList() << "-qt5" << "-print-env") << (QStringList() << "-print-env");
    QTest::newRow("run-tool") << (QStringList() << "-run-tool=foobar") << (QStringList() << "-run-tool=foobar");
    QTest::newRow("qt5-run-tool") << (QStringList() << "-qt5" << "-run-tool=foobar") << (QStringList() << "-run-tool=foobar");
}

void tst_ToolChooser::passArgs()
{
    QFETCH(QStringList, args);
    QFETCH(QStringList, expected);

    QProcessEnvironment env = testModeEnvironment;
    env.insert("QTCHOOSER_RUNTOOL", "testtool");
    QScopedPointer<QProcess> proc(execute(args, env));
    VERIFY_NORMAL_EXIT(proc);

    // skip the first line of procstdout, as it contains the tool name
    proc->readLine();

    QByteArray procstdout = proc->readAll().trimmed();
    QCOMPARE(QString::fromLocal8Bit(procstdout), expected.join("\n"));
}

void tst_ToolChooser::install_data()
{
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<QString>("expectedName");

    QTest::newRow("missing-name") << QStringList() << QString();
    QTest::newRow("missing-qmake") << (QStringList() << "sdk") << QString();

    QString qmake = QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmake";
    QVERIFY(QFile::exists(qmake));

    QStringList baseArgs;
    baseArgs << "5" << qmake;

    QTest::newRow("global-wouldoverwrite") << baseArgs << QString();
    QTest::newRow("local-wouldoverwrite") << (QStringList() << "-local" << baseArgs) << QString();

    baseArgs.prepend("-f");
    QTest::newRow("global-overwrite") << baseArgs << testData + "/config2/qtchooser/5.conf";
    QTest::newRow("local-overwrite") << (QStringList() << "-local" << baseArgs) << "/dev/null/qtchooser/5.conf";

    baseArgs.clear();
    baseArgs << "newname" << qmake;
    QTest::newRow("global-newname") << baseArgs << testData + "/config2/qtchooser/newname.conf";
    QTest::newRow("local-newname") << (QStringList() << "-local" << baseArgs) << "/dev/null/qtchooser/newname.conf";

    // ensure that we find an SDK in a later path, even if we could install on an earlier one
    QTest::newRow("global-wouldoverwrite-later") << (QStringList() << "later" << qmake) << QString();
}

void tst_ToolChooser::install()
{
    QFETCH(QStringList, args);
    QFETCH(QString, expectedName);

    QProcessEnvironment env = testModeEnvironment;
    QScopedPointer<QProcess> proc(execute((QStringList() << "-install") + args, env));
    QVERIFY(!!proc);
    if (expectedName.isEmpty()) {
        QByteArray err = proc->readAllStandardError();
        QVERIFY(!err.isEmpty());
        QVERIFY(proc->exitCode() != 0);
        qDebug() << err.trimmed();
    } else {
        VERIFY_NORMAL_EXIT(proc);

        QByteArray out = proc->readLine();
        QVERIFY(!out.isEmpty());
        QCOMPARE(QString(out).trimmed(), expectedName);

        out = proc->readLine();
        QCOMPARE(QString(out).trimmed(), QLibraryInfo::location(QLibraryInfo::BinariesPath));

        out = proc->readLine();
        QCOMPARE(QString(out).trimmed(), QLibraryInfo::location(QLibraryInfo::LibrariesPath));
    }
}

void tst_ToolChooser::install2()
{
    // verify that the root is not writable by the current user
    QString root = QDir::rootPath();
    {
        QFile f(root + "qtchooser");
        QVERIFY(!f.exists());
        QVERIFY(!f.open(QIODevice::ReadWrite));
    }

    QTemporaryDir tempdir;
    QDir dir(tempdir.path());
    dir.mkdir("/global");
    dir.mkdir("/home");

    QString realToolPath = QCoreApplication::applicationDirPath() + "/../../../src/qtchooser/qtchooser" EXE_SUFFIX;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("XDG_CONFIG_HOME");
    env.insert("XDG_CONFIG_DIRS", tempdir.path() + "/global/etc/xdg" LIST_SEP "/");
    env.insert("HOME", tempdir.path() + "/home");

    QProcess proc;
    proc.setProcessEnvironment(env);
    QString qmake = QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmake";
    QString expectedContents = QLibraryInfo::location(QLibraryInfo::BinariesPath) + '\n' +
                               QLibraryInfo::location(QLibraryInfo::LibrariesPath) + '\n';

    // test 1: check that it installs into $HOME and recursively mkdirs
    proc.setProgram(realToolPath);
    proc.setArguments(QStringList() << "-install" << "-local" << "test" << qmake);
    proc.start();
    QVERIFY(proc.waitForFinished());
    VERIFY_NORMAL_EXIT(&proc);

    // find the file it must've created
    {
        QFile f(tempdir.path() + "/home/.config/qtchooser/test.conf");
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
        QCOMPARE(f.readAll(), expectedContents.toLocal8Bit());
    }
    QVERIFY(!QFile::exists(tempdir.path() + "/global/etc/xdg/qtchooser/test.conf"));

    // test 2: check that it can create a global override
    proc.setArguments(QStringList() << "-install" << "-f" << "test" << qmake);
    proc.start();
    QVERIFY(proc.waitForFinished());
    VERIFY_NORMAL_EXIT(&proc);

    // find the global file
    {
        QFile f(tempdir.path() + "/global/etc/xdg/qtchooser/test.conf");
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
        QCOMPARE(f.readAll(), expectedContents.toLocal8Bit());
    }
}

QTEST_MAIN(tst_ToolChooser)

#include "tst_qtchooser.moc"
