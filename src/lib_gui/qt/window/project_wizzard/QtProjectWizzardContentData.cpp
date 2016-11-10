#include "qt/window/project_wizzard/QtProjectWizzardContentData.h"

#include <QFormLayout>
#include <QMessageBox>

#include "settings/CxxProjectSettings.h"

#include "utility/messaging/type/MessageIDECreateCDB.h"
#include "utility/logging/logging.h"

QtProjectWizzardContentData::QtProjectWizzardContentData(std::shared_ptr<ProjectSettings> settings, QtProjectWizzardWindow* window)
	: QtProjectWizzardContent(settings, window)
	, m_projectName(nullptr)
	, m_projectFileLocation(nullptr)
	, m_language(nullptr)
	, m_standard(nullptr)
	, m_buildFilePicker(nullptr)
{
}

void QtProjectWizzardContentData::populate(QGridLayout* layout, int& row)
{
	if (!isInForm())
	{
		layout->setRowMinimumHeight(row, 15);
		row++;
	}

	addNameAndLocation(layout, row);

	if (!isInForm())
	{
		layout->setRowMinimumHeight(row++, 20);
	}

	addLanguageAndStandard(layout, row);

	if (!isInForm())
	{
		layout->setRowMinimumHeight(row, 15);
		layout->setRowStretch(row, 1);
	}
}

void QtProjectWizzardContentData::load()
{
	if (m_projectName)
	{
		m_projectName->setText(QString::fromStdString(m_settings->getProjectName()));
		m_projectFileLocation->setText(QString::fromStdString(m_settings->getProjectFileLocation().str()));
	}

	if (m_language)
	{
		LanguageType type = m_settings->getLanguage();

		if (type == LANGUAGE_UNKNOWN)
		{
			LOG_ERROR("No language type defined");
			return;
		}

		m_language->setText(languageTypeToString(type).c_str());

		m_standard->clear();
		std::vector<std::string> standards = m_settings->getLanguageStandards();
		for (size_t i = 0; i < standards.size(); i++)
		{
			m_standard->insertItem(i, standards[i].c_str());
		}

		m_standard->setCurrentText(QString::fromStdString(m_settings->getStandard()));
	}
}

void QtProjectWizzardContentData::save()
{
	if (m_projectName)
	{
		m_settings->setProjectName(m_projectName->text().toStdString());
		m_settings->setProjectFileLocation(m_projectFileLocation->getText().toStdString());
	}

	if (m_standard)
	{
		m_settings->setStandard(m_standard->currentText().toStdString());
	}
}

bool QtProjectWizzardContentData::check()
{
	if (!m_projectName)
	{
		return true;
	}

	if (m_projectName->text().isEmpty())
	{
		QMessageBox msgBox;
		msgBox.setText("Please enter a project name.");
		msgBox.exec();
		return false;
	}

	if (m_projectFileLocation->getText().isEmpty())
	{
		QMessageBox msgBox;
		msgBox.setText("Please define the location of the project file.");
		msgBox.exec();
		return false;
	}

	if (!FilePath(m_projectFileLocation->getText().toStdString()).expandEnvironmentVariables().exists())
	{
		QMessageBox msgBox;
		msgBox.setText("The specified location does not exist.");
		msgBox.exec();
		return false;
	}

	return true;
}

void QtProjectWizzardContentData::addNameAndLocation(QGridLayout* layout, int& row)
{
	QLabel* nameLabel = createFormLabel("Project Name");
	m_projectName = new QLineEdit();
	m_projectName->setObjectName("name");
	m_projectName->setAttribute(Qt::WA_MacShowFocusRect, 0);

	layout->addWidget(nameLabel, row, QtProjectWizzardWindow::FRONT_COL, Qt::AlignRight);
	layout->addWidget(m_projectName, row, QtProjectWizzardWindow::BACK_COL);
	row++;


	QLabel* locationLabel = createFormLabel("Project File Location");
	m_projectFileLocation = new QtLocationPicker(this);
	m_projectFileLocation->setPickDirectory(true);

	layout->addWidget(locationLabel, row, QtProjectWizzardWindow::FRONT_COL, Qt::AlignRight);
	layout->addWidget(m_projectFileLocation, row, QtProjectWizzardWindow::BACK_COL, Qt::AlignTop);
	layout->setRowMinimumHeight(row, 30);
	row++;
}

void QtProjectWizzardContentData::addLanguageAndStandard(QGridLayout* layout, int& row)
{
	m_language = new QLabel();
	layout->addWidget(createFormLabel("Language"), row, QtProjectWizzardWindow::FRONT_COL, Qt::AlignRight);
	layout->addWidget(m_language, row, QtProjectWizzardWindow::BACK_COL, Qt::AlignLeft);
	row++;

	m_standard = new QComboBox();
	layout->addWidget(createFormLabel("Standard"), row, QtProjectWizzardWindow::FRONT_COL, Qt::AlignRight);
	layout->addWidget(m_standard, row, QtProjectWizzardWindow::BACK_COL, Qt::AlignLeft);
	row++;
}

void QtProjectWizzardContentData::addBuildFilePicker(
	QGridLayout* layout, int& row, const QString& name, const QString& filter)
{
	QLabel* label = createFormLabel(name);
	layout->addWidget(label, row, QtProjectWizzardWindow::FRONT_COL);

	m_buildFilePicker = new QtLocationPicker(this);
	m_buildFilePicker->setFileFilter(filter);

	layout->addWidget(m_buildFilePicker, row, QtProjectWizzardWindow::BACK_COL);

	row++;

	QLabel* description = new QLabel(
		"Coati will use all include paths and compiler flags from the compilation database and stay up-to-date "
		"with changes on refresh.", this);
	description->setObjectName("description");
	description->setWordWrap(true);
	layout->addWidget(description, row, QtProjectWizzardWindow::BACK_COL);

	row++;
}


QtProjectWizzardContentDataCDB::QtProjectWizzardContentDataCDB(
	std::shared_ptr<ProjectSettings> settings, QtProjectWizzardWindow* window)
	: QtProjectWizzardContentData(settings, window)
{
}

void QtProjectWizzardContentDataCDB::populate(QGridLayout* layout, int& row)
{
	if (!isInForm())
	{
		layout->setRowMinimumHeight(row, 15);
		row++;
	}

	addNameAndLocation(layout, row);

	layout->setRowMinimumHeight(row++, 20);

	QString name = "Compilation Database (compile_commands.json)";
	QString filter = "JSON Compilation Database (*.json)";
	addBuildFilePicker(layout, row, name, filter);

	if (!isInForm())
	{
		layout->setRowMinimumHeight(row, 15);
		layout->setRowStretch(row, 1);
	}
}

void QtProjectWizzardContentDataCDB::load()
{
	QtProjectWizzardContentData::load();
	std::shared_ptr<CxxProjectSettings> cxxSettings = std::dynamic_pointer_cast<CxxProjectSettings>(m_settings);
	if (cxxSettings)
	{
		m_buildFilePicker->setText(QString::fromStdString(cxxSettings->getCompilationDatabasePath().str()));
	}
}

void QtProjectWizzardContentDataCDB::save()
{
	QtProjectWizzardContentData::save();

	FilePath path = m_buildFilePicker->getText().toStdString();
	if (!path.exists() || path.extension() != ".json")
	{
		return;
	}

	std::shared_ptr<CxxProjectSettings> cxxSettings = std::dynamic_pointer_cast<CxxProjectSettings>(m_settings);
	if (cxxSettings)
	{
		cxxSettings->setCompilationDatabasePath(path);
	}
}

bool QtProjectWizzardContentDataCDB::check()
{
	if (!QtProjectWizzardContentData::check())
	{
		return false;
	}

	FilePath path = m_buildFilePicker->getText().toStdString();
	if (!path.exists() || path.extension() != ".json")
	{
		QMessageBox msgBox;
		msgBox.setText("Please enter a valid compilation database file (*.json).");
		msgBox.exec();
		return false;
	}

	return true;
}

void QtProjectWizzardContentDataCDB::refreshClicked()
{
}




QtProjectWizzardContentDataCDBVS::QtProjectWizzardContentDataCDBVS(std::shared_ptr<ProjectSettings> settings, QtProjectWizzardWindow* window)
	: QtProjectWizzardContentDataCDB(settings, window)
{
}

void QtProjectWizzardContentDataCDBVS::populate(QGridLayout* layout, int& row)
{
	if (!isInForm())
	{
		layout->setRowMinimumHeight(row, 15);
		row++;
	}

	QLabel* nameLabel = createFormLabel("Create Compilation Database");
	layout->addWidget(nameLabel, row, QtProjectWizzardWindow::FRONT_COL);

	addHelpButton("To create a new Compilation Database from a Visual Studio Solution, this Solution has to be open in Visual Studio.\n\
Coati will call Visual Studio to open the 'Create Compilation Database' dialog.\
 Please follow the instructions in Visual Studio to complete the process.\n\
Note: Coati's Visual Studio plugin has to be installed. Visual Studio has to be running with an eligible Solution, containing C/C++ projects, loaded.", layout, row);

	QLabel* descriptionLabel = createFormLabel("Call Visual Studio to create a Compilation Database from the loaded Solution.");
	descriptionLabel->setObjectName("description");
	descriptionLabel->setAlignment(Qt::AlignmentFlag::AlignLeft);
	layout->addWidget(descriptionLabel, row, QtProjectWizzardWindow::BACK_COL);
	row++;

	QPushButton* button = new QPushButton("Create CDB");
	button->setObjectName("windowButton");
	layout->addWidget(button, row, QtProjectWizzardWindow::BACK_COL);
	row++;

	QLabel* skipLabel = createFormLabel("*Skip this step if you already have a Compilation Database for your Solution.");
	skipLabel->setObjectName("description");
	skipLabel->setAlignment(Qt::AlignmentFlag::AlignLeft);
	layout->addWidget(skipLabel, row, QtProjectWizzardWindow::BACK_COL);
	row++;
	

	QFrame* separator = new QFrame();
	separator->setFrameShape(QFrame::HLine);

	QPalette palette = separator->palette();
	palette.setColor(QPalette::WindowText, Qt::lightGray);
	separator->setPalette(palette);

	layout->addWidget(separator, row++, 0, 1, -1);


	connect(button, SIGNAL(clicked()), this, SLOT(handleVSCDBClicked()));

	addNameAndLocation(layout, row);

	layout->setRowMinimumHeight(row++, 20);

	QString name = "Compilation Database";
	QString filter = "JSON Compilation Database (*.json)";
	addBuildFilePicker(layout, row, name, filter);

	if (!isInForm())
	{
		layout->setRowMinimumHeight(row, 15);
		layout->setRowStretch(row, 1);
	}
}

void QtProjectWizzardContentDataCDBVS::refreshClicked()
{
}

void QtProjectWizzardContentDataCDBVS::handleVSCDBClicked()
{
	MessageIDECreateCDB().dispatch();
}
