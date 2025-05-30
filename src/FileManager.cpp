#include "FileManager.h"
#include "CodeEditor.h"
#include "MainWindow.h"
#include "SyntaxManager.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFileInfo>
#include <filesystem>
#include <iostream>
#include <fstream>

FileManager::FileManager(CodeEditor *editor, MainWindow *mainWindow)
    : m_editor(editor), m_mainWindow(mainWindow)
{
    qDebug() << "FileManager initialized.";
}

FileManager::~FileManager() {}

void FileManager::initialize(CodeEditor *editor, MainWindow *mainWindow)
{
    m_editor     = editor;
    m_mainWindow = mainWindow;
}

QString FileManager::getCurrentFileName() const
{
    return m_currentFileName;
}

void FileManager::setCurrentFileName(const QString fileName)
{
    m_currentFileName = fileName;
}

void FileManager::newFile()
{
    // Logic to create a new file
}

void FileManager::saveFile()
{
    if (m_currentFileName.isEmpty())
    {
        saveFileAs();
        return;
    }

    qDebug() << "Saving file:" << m_currentFileName;

    QFile file(m_currentFileName);
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        QMessageBox::warning(nullptr, "Error", "Cannot save file: " + file.errorString());
        return;
    }

    QTextStream out(&file);
    if (m_editor)
    {
        out << m_editor->toPlainText();
    }
    else
    {
        QMessageBox::critical(nullptr, "Error", "Editor is not initialized.");
        return;
    }
    file.close();

    emit m_editor->statusMessageChanged("File saved successfully.");
}

void FileManager::saveFileAs()
{
    QString fileExtension = getFileExtension();
    QString filter        = "All Files (*);;C++ Files (*.cpp *.h);;Text Files (*.txt)";
    if (!fileExtension.isEmpty())
    {
        filter = QString("%1 Files (*.%2);;%3").arg(fileExtension.toUpper(), fileExtension, filter);
    }

    QString fileName = QFileDialog::getSaveFileName(nullptr, "Save File As", QString(), filter);

    if (!fileName.isEmpty())
    {
        m_currentFileName = fileName;
        saveFile();
    }
}

void FileManager::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(nullptr, "Open File", QString(),
                                                    "All Files (*);;C++ Files (*.cpp *.h);;Text Files (*.txt)");
    if (!fileName.isEmpty())
    {
        qDebug() << "Opening file: " << fileName;
        m_currentFileName = fileName;
        loadFileInEditor(fileName);
    }
    else
    {
        qDebug() << "No file selected.";
    }
}

void FileManager::loadFileInEditor(const QString &filePath)
{
    qDebug() << "Loading file:" << filePath;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(nullptr, "Error", "Cannot open file: " + file.errorString());
        return;
    }

    QTextStream in(&file);
    if (m_editor)
    {
        m_editor->setPlainText(in.readAll());

        delete m_currentHighlighter;

        // Create and assign a new syntax highlighter based on language extension
        m_currentHighlighter = SyntaxManager::createSyntaxHighlighter(getFileExtension(), m_editor->document()).release();
    }
    else
    {
        QMessageBox::critical(nullptr, "Error", "Editor is not initialized.");
        return;
    }
    file.close();

    if (m_mainWindow)
    {
        m_mainWindow->setWindowTitle("CodeAstra ~ " + QFileInfo(filePath).fileName());
    }
    else
    {
        qWarning() << "MainWindow is not initialized in FileManager.";
    }
}

QString FileManager::getFileExtension() const
{
    if (m_currentFileName.isEmpty())
    {
        qDebug() << "Error: No File name set!";
        return QString();
    }

    return QFileInfo(m_currentFileName).suffix().toLower();
}

QString FileManager::getDirectoryPath() const
{
    return QFileDialog::getExistingDirectory(
        nullptr, QObject::tr("Open Directory"), QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}

// Check path to prevent path traversal attack
bool isValidPath(const std::filesystem::path &path)
{
    std::string pathStr = path.string();
    if (pathStr.find("..") != std::string::npos)
    {
        return false;
    }

    return true;
}

OperationResult FileManager::renamePath(const QFileInfo &pathInfo, const QString &newName)
{
    if (!pathInfo.exists())
    {
        return {false, "Path does not exist: " + pathInfo.fileName().toStdString()};
    }

    std::filesystem::path oldPath = pathInfo.absoluteFilePath().toStdString();

    // Validate the input path
    if (!isValidPath(oldPath))
    {
        return {false, "Invalid file path."};
    }

    std::filesystem::path newPath = oldPath.parent_path() / newName.toStdString();

    if (QFileInfo(newPath).exists())
    {
        return {false, newPath.filename().string() + " already takken."};
    }

    try
    {
        std::filesystem::rename(oldPath, newPath);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        QMessageBox::critical(nullptr, "Error", QString(e.what()));
        return {false, e.what()};
    }

    return {true, newPath.filename().string()};
}

// Check if the path is a valid directory
// and not a system or home directory
bool isAValidDirectory(const QFileInfo &pathInfo)
{
    if (!pathInfo.exists())
    {
        qWarning() << "Path does not exist: " << pathInfo.fileName();
        return false;
    }

    if (pathInfo.absolutePath() == "/" || pathInfo.absolutePath() == QDir::homePath())
    {
        QMessageBox::critical(nullptr, "Error", "Cannot delete system or home directory.");
        return false;
    }

    return true;
}

OperationResult FileManager::deletePath(const QFileInfo &pathInfo)
{
    if (!isAValidDirectory(pathInfo))
    {
        return {false, "ERROR: invalid folder path." + pathInfo.absolutePath().toStdString()};
    }

    std::filesystem::path pathToDelete = pathInfo.absoluteFilePath().toStdString();

    // Validate the input path
    if (!isValidPath(pathToDelete))
    {
        return {false, "ERROR: invalid file path." + pathToDelete.filename().string()};
    }

    if (!QFile::moveToTrash(pathToDelete))
    {
        return {false, "ERROR: failed to delete: " + pathToDelete.string()};
    }

    return {true, pathToDelete.filename().string()};
}

OperationResult FileManager::newFile(const QFileInfo &pathInfo, QString newFilePath)
{
    std::filesystem::path dirPath = pathInfo.absolutePath().toStdString();

    if (pathInfo.isDir())
    {
        dirPath = pathInfo.absoluteFilePath().toStdString();
    }

    if (!isValidPath(dirPath))
    {
        return {false, "invalid file path."};
    }

    std::filesystem::path filePath = dirPath / newFilePath.toStdString();
    if (QFileInfo(filePath).exists())
    {
        return {false, filePath.filename().string() + " already used."};
    }

    std::ofstream file(filePath);
    if (file.is_open())
    {
        file.close();
    }
    qDebug() << "New file created.";

    FileManager::getInstance().setCurrentFileName(QString::fromStdString(filePath.string()));
    return {true, filePath.filename().string()};
}

OperationResult FileManager::newFolder(const QFileInfo &pathInfo, QString newFolderPath)
{
    // TO-DO: look up which is prefered: error_code or exception
    std::error_code err{};
    std::filesystem::path dirPath = pathInfo.absolutePath().toStdString();

    // Check if the path is a directory
    if (pathInfo.isDir())
    {
        dirPath = pathInfo.absoluteFilePath().toStdString();
    }

    // Validate the input path
    if (!isValidPath(dirPath))
    {
        return {false, "Invalid file path."};
    }

    std::filesystem::path newPath = dirPath / newFolderPath.toStdString();
    if (QFileInfo(newPath).exists())
    {
        return {false, newPath.filename().string() + " already used."};
    }

    std::filesystem::create_directory(newPath, err);
    if (err)
    {
        qDebug() << "Error creating directory:" << QString::fromStdString(err.message());
        return {false, err.message().c_str()};
    }

    qDebug() << "New folder created at:" << QString::fromStdString(newPath.string());

    return {true, newPath.filename().string()};
}

OperationResult FileManager::duplicatePath(const QFileInfo &pathInfo)
{
    std::filesystem::path filePath = pathInfo.absoluteFilePath().toStdString();

    // Validate the input path
    if (!isValidPath(filePath))
    {
        return {false , "Invalid path."};
    }

    std::string fileName           = filePath.stem().string();
    std::filesystem::path dupPath  = filePath.parent_path() / (fileName + "_copy" + filePath.extension().c_str());

    int counter = 1;
    while (QFileInfo(dupPath).exists())
    {
        dupPath = filePath.parent_path() / (fileName + "_copy" + std::to_string(counter) + filePath.extension().c_str());
        counter++;
    }

    try
    {
        std::filesystem::copy(filePath, dupPath, std::filesystem::copy_options::recursive); // copy_option is needed for duplicating nested directories
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        return {false, e.what()};
    }

    qDebug() << "Duplicated file to:" << QString::fromStdString(dupPath.string());

    return {true, dupPath.filename().string()};
}