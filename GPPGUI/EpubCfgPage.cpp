#include "EpubCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaColorDialog.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ValueSliderWidget.h"
#include "ElaText.h"

import Tool;

EpubCfgPage::EpubCfgPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle("Epub 输出配置");
	setContentsMargins(10, 0, 10, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 双语显示
	bool bilingual = _projectConfig["plugins"]["Epub"]["双语显示"].value_or(true);
	ElaScrollPageArea* outputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* outputLayout = new QHBoxLayout(outputArea);
	ElaText* outputText = new ElaText("双语显示", outputArea);
	outputText->setTextPixelSize(16);
	outputLayout->addWidget(outputText);
	outputLayout->addStretch();
	ElaToggleSwitch* outputSwitch = new ElaToggleSwitch(outputArea);
	outputSwitch->setIsToggled(bilingual);
	outputLayout->addWidget(outputSwitch);
	mainLayout->addWidget(outputArea);

	// 原文颜色
	std::string colorStr = _projectConfig["plugins"]["Epub"]["原文颜色"].value_or("#808080");
	QColor color = QColor(colorStr.c_str());
	ElaScrollPageArea* colorArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* colorLayout = new QHBoxLayout(colorArea);
	ElaText* colorDialogText = new ElaText("原文颜色", colorArea);
	colorDialogText->setTextPixelSize(16);
	colorLayout->addWidget(colorDialogText);
	colorLayout->addStretch();
	ElaColorDialog* colorDialog = new ElaColorDialog(colorArea);
	colorDialog->setCurrentColor(color);
	ElaText* colorText = new ElaText(colorDialog->getCurrentColorRGB(), colorArea);
	colorText->setTextPixelSize(15);
	ElaPushButton* colorButton = new ElaPushButton(colorArea);
	colorButton->setFixedSize(35, 35);
	colorButton->setLightDefaultColor(colorDialog->getCurrentColor());
	colorButton->setLightHoverColor(colorDialog->getCurrentColor());
	colorButton->setLightPressColor(colorDialog->getCurrentColor());
	colorButton->setDarkDefaultColor(colorDialog->getCurrentColor());
	colorButton->setDarkHoverColor(colorDialog->getCurrentColor());
	colorButton->setDarkPressColor(colorDialog->getCurrentColor());
	connect(colorButton, &ElaPushButton::clicked, this, [=]() {
		colorDialog->exec();
		});
	connect(colorDialog, &ElaColorDialog::colorSelected, this, [=](const QColor& color) {
		colorButton->setLightDefaultColor(color);
		colorButton->setLightHoverColor(color);
		colorButton->setLightPressColor(color);
		colorButton->setDarkDefaultColor(color);
		colorButton->setDarkHoverColor(color);
		colorButton->setDarkPressColor(color);
		colorText->setText(colorDialog->getCurrentColorRGB());
		});
	colorLayout->addWidget(colorButton);
	colorLayout->addWidget(colorText);
	mainLayout->addWidget(colorArea);


	// 缩小比例
	double scale = _projectConfig["plugins"]["Epub"]["缩小比例"].value_or(0.8);
	ElaScrollPageArea* scaleArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* scaleLayout = new QHBoxLayout(scaleArea);
	ElaText* scaleText = new ElaText("缩小比例", scaleArea);
	scaleText->setTextPixelSize(16);
	scaleLayout->addWidget(scaleText);
	scaleLayout->addStretch();
	ValueSliderWidget* scaleSlider = new ValueSliderWidget(scaleArea);
	scaleSlider->setDecimals(2);
	scaleSlider->setValue(scale);
	scaleLayout->addWidget(scaleSlider);
	mainLayout->addWidget(scaleArea);

	// 预处理正则
	toml::table preRegexTable = toml::table{};
	auto preRegexArr = _projectConfig["plugins"]["Epub"]["预处理正则"].as_array();
	if (preRegexArr) {
		for (auto& elem : *preRegexArr) {
			auto tbl = elem.as_table();
			if (!tbl) {
				continue;
			}
			tbl->is_inline(false);
		}
		preRegexTable.insert("预处理正则", *preRegexArr);
	}
	else {
		preRegexTable.insert("预处理正则", toml::array{});
	}
	ElaText* preRegexText = new ElaText("预处理正则", centerWidget);
	preRegexText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(preRegexText);
	ElaPlainTextEdit* preRegexEdit = new ElaPlainTextEdit(centerWidget);
	preRegexEdit->setMinimumHeight(300);
	preRegexEdit->setPlainText(QString::fromStdString(stream2String(preRegexTable)));
	mainLayout->addWidget(preRegexEdit);

	// 后处理正则
	toml::table postRegexTable = toml::table{};
	auto postRegexArr = _projectConfig["plugins"]["Epub"]["后处理正则"].as_array();
	if (postRegexArr) {
		for (auto& elem : *postRegexArr) {
			auto tbl = elem.as_table();
			if (!tbl) {
				continue;
			}
			tbl->is_inline(false);
		}
		postRegexTable.insert("后处理正则", *postRegexArr);
	}
	else {
		postRegexTable.insert("后处理正则", toml::array{});
	}
	ElaText* postRegexText = new ElaText("后处理正则", centerWidget);
	postRegexText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(postRegexText);
	ElaPlainTextEdit* postRegexEdit = new ElaPlainTextEdit(centerWidget);
	postRegexEdit->setMinimumHeight(300);
	toml::toml_formatter formatter{ postRegexTable , {toml::format_flags::none} };
	std::stringstream ss;
	ss << formatter;
	postRegexEdit->setPlainText(QString::fromStdString(ss.str()));
	mainLayout->addWidget(postRegexEdit);

	ElaText* tipText = new ElaText("说明", centerWidget);
	tipText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(tipText);
	ElaPlainTextEdit* tipEdit = new ElaPlainTextEdit(centerWidget);
	tipEdit->setMinimumHeight(400);
	tipEdit->setReadOnly(true);
	tipEdit->setPlainText(R"(鉴于Epub的多样性以及GalTransl++以项目为本的理念，GalTransl++的Epub提取并不像其它翻译器一样有固定的解析/拼装模式。
GalTransl++将仅使用GUMBO库遍历Epub中的HTML/XHTML文件
(epub文件本身只是一个zip包，你可以使用任何解压软件打开它并浏览其中的文件)，每个文本标签提取出的字符串都将作为一个msg。
但仅仅这样会有一些明显的问题，如下面这个句子: 

<p class="class_s2C-0">「モテる男は<ruby><rb>辛</rb><rt>つら</rt></ruby>いね」</p>

它会被拆解为 「モテる男は 辛 つら いね」四个部分，作为4个msg喂给AI，这显然不是我们想要的。
如果使用固定的html解析方法/固定的正则组进行提取，要么会丢失信息，要么无法兼顾所有情况，总之自定义程度极低，最后输出的效果理想与否完全取决于程序的解析与拼装模式是否正好符合用户的预期。
所以GalTransl++选择让用户根据自己的项目自行利用正则构建解析模式，这显然增长了一些门槛，不过作为回报大幅提升了自由度。

Epub正则设置依然使用toml语法解析配置，并支持两种正则写法: 一般正则和回调正则。
一般正则执行简单的匹配替换，例如使用下面的预处理正则，

[[plugins.Epub.'预处理正则']]
org = '<ruby><rb>(.+?)</rb><rt>(.+?)</rt></ruby>'
rep = '[$2/$1]'

则上面的句子会被替换为: 

<p class="class_s2C-0">「モテる男は[つら/辛]いね」</p>

之后再遍历到此处时，「モテる男は[つら/辛]いね」 将被作为一个完整的句子被提取出来，
如果之后搭配如下后处理正则，

[[plugins.Epub.'后处理正则']]
org = '\[([^/\[\]]+?)/([^/\[\]]+?)\]'
rep = '<ruby><rb>$2</rb><rt>$1</rt></ruby>'

则可在理想位置还原振假名效果(可以修改提示词告诉AI注音格式[振假名/基本文本]及保留要求来达到更好的保留效果)。
当然如果只想在原文中保留振假名而在译文中删去，那也很简单，直接在译前字典中再写一个去注音字典即可。
其它很多自定义需求也都可借此满足。

不过如上正则仍有一些缺陷，比如这两句话: 

<p class="class_s2C-0">「うっ、レナ<span class="class_s2R">!?</span>」</p>
<p class="class_s2C-0">　二人の女の子が火花を散らす。どちらからの好意も嬉しくて、ついつい甘んじてし<span id="page_8"/>まう。</p>

假如我不想一个一个文件的看标签写正则，
而是想要直接『删除所有<p></p>标签内的其余标签并保留非标签部分』来快速过滤的话，
面对不擅长处理嵌套的简单正则，显然我们很难写出这样的正则/正则组来处理这个问题，
那么此时就需要使用回调正则。

我们可以编写如下回调正则，

[[plugins.Epub.'预处理正则']]
org = '(<p[^>/]*>)(.*?)(</p>)'
callback = [ { group = 2, org = '<[^>]*>', rep = '' } ]

则这两句话会被替换为

<p class="class_s2C-0">「うっ、レナ!?」</p>
<p class="class_s2C-0">　二人の女の子が火花を散らす。どちらからの好意も嬉しくて、ついつい甘んじてしまう。</p>

回调正则的语法是，处理每个匹配项中的所有捕获组，对每个捕获组使用其对应group的回调正则/正则组进行替换，
最后将所有捕获组按顺序合并作为这个匹配项的替换字符串。

采用第一句话作为例子，第一句话 <p class="class_s2C-0">「うっ、レナ<span class="class_s2R">!?</span>」</p> 整句被正则 
'(<p[^>/]*>)(.*?)(</p>)' 匹配到，则这一整句作为一个匹配项。
在这个匹配项中，根据捕获组又可以分为三组(没有被分组的部分则直接忽略)
第一组: <p class="class_s2C-0">
第二组: 「うっ、レナ<span class="class_s2R">!?</span>」
第三组: </p>

对于第一组文本，程序会寻找所有 group = 1 的回调正则，并依次执行替换。
显然上面的回调正则并没有 group = 1 的正则，则忽略替换，第一组原样输出，
现在的替换字符串暂定为: <p class="class_s2C-0">

对于第二组文本，程序找到了 group = 2 的正则，则对第二组文本使用 org = '<[^>]*>', rep = '' 进行替换，
替换后得到 「うっ、レナ!?」 ，则现在的替换字符串暂定为 <p class="class_s2C-0">「うっ、レナ!?」

第三组文本与第一组文本同理，则最终的替换字符串为 <p class="class_s2C-0">「うっ、レナ!?」</p> ，与预期相符。)");
	mainLayout->addWidget(tipEdit);

	_applyFunc = [=]()
		{
			insertToml(_projectConfig, "plugins.Epub.双语显示", outputSwitch->getIsToggled());
			insertToml(_projectConfig, "plugins.Epub.原文颜色", colorDialog->getCurrentColorRGB().toStdString());
			insertToml(_projectConfig, "plugins.Epub.缩小比例", scaleSlider->value());

			try {
				toml::table preTbl = toml::parse(preRegexEdit->toPlainText().toStdString());
				auto preArr = preTbl["预处理正则"].as_array();
				if (preArr) {
					insertToml(_projectConfig, "plugins.Epub.预处理正则", *preArr);
				}
				else {
					insertToml(_projectConfig, "plugins.Epub.预处理正则", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "Epub预处理正则格式错误", 3000);
			}
			try {
				toml::table postTbl = toml::parse(postRegexEdit->toPlainText().toStdString());
				auto postArr = postTbl["后处理正则"].as_array();
				if (postArr) {
					insertToml(_projectConfig, "plugins.Epub.后处理正则", *postArr);
				}
				else {
					insertToml(_projectConfig, "plugins.Epub.后处理正则", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "Epub后处理正则格式错误", 3000);
			}
		};
	
	mainLayout->addStretch();
	centerWidget->setWindowTitle("Epub 输出配置");
	addCentralWidget(centerWidget, true, true, 0);
}

EpubCfgPage::~EpubCfgPage()
{

}
