#include "SkipTransCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QByteArray>

#include "ElaToggleSwitch.h"
#include "ElaText.h"
#include "ElaToolTip.h"
#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"
#include "ElaDoubleText.h"
#include "ElaPushButton.h"
#include "ElaWidget.h"
#include "TreeSitterHighlighter.h"

import Tool;

constexpr auto hKeysBase64Default =
"M1AKQVblpbPlhKoKR+OCueODneODg+ODiApOVFIKU0VYClNNClNPRApU44OQ44OD44KvCuOBhOOChOOCieOBl+"
"OBhArjgYjjgaPjgaEK44GK44Gh44KT44Gh44KTCuOBiuOBo8+ACuOBiuOBo+OBseOBhArjgYrjgarjgavjg7wK"
"44GK44Gt44K344On44K/CuOBiuOBvOOBkwrjgYrjgb7jgpPjgZMK44GK44KB44GTCuOBiuaOg+mZpOODleOCp+"
"ODqQrjgY3jgpPjgZ/jgb4K44GV44GL44GV5qSL6bOlCuOBl+OBvOOCiuiKmeiTiQrjgZnjgZHjgbkK44Gb44G"
"N44KM44GE5pys5omLCuOBm+OBo+OBj+OBmQrjgaDjgYTjgZfjgoXjgY3jg5vjg7zjg6vjg4kK44Gh44KT44GT"
"CuOBoeOCk+OBoeOCkwrjgaHjgpPjgb0K44Gy44Go44KK44GI44Gj44GhCuOBteOBn+OBquOCigrjgb7jgpPjg"
"ZDjgorov5TjgZcK44G+44KT44GTCuOBvuOCk+OBvuOCkwrjgoDjgonjgoDjgokK44Ki44Kv44OhCuOCouOCsu"
"ODnuODswrjgqLjg4Djg6vjg4jjg5Pjg4fjgqoK44Ki44OK44OL44O8CuOCouODiuODqwrjgqLjg4rjg6vjgrv"
"jg4Pjgq/jgrkK44Ki44OK44Or44OT44O844K6CuOCouODiuODq+ODl+ODqeOCsArjgqLjg4rjg6vmi6HlvLUK"
"44Ki44OK44Or6ZaL55m6CuOCouODiuODq++8s++8pe+8uArjgqLjg5jpoZQK44Kk44KvCuOCpOODgeODouODh"
"ArjgqTjg4Hjg6PjgqTjg4Hjg6Pjgrvjg4Pjgq/jgrkK44Kk44OB44Oj44Op44OW44K744OD44Kv44K5CuOCpO"
"ODoeOCr+ODqQrjgqTjg6Hjg7zjgrjjg5Pjg4fjgqoK44Kk44Op44Oe44OB44KqCuOCpOODs+ODnQrjgqTjg7P"
"jg53jg4bjg7Pjg4QK44Ko44Kv44K544K/44K344O8CuOCqOODg+ODgQrjgqjjg60K44Ko44Ot44GECuOCqOOD"
"reWQjOS6ugrjgqjjg63lkIzkurroqowK44Ko44Ot5pysCuOCquODiuODi+ODvArjgqrjg4rjg5oK44Kq44OK4"
"4Oa44OD44OICuOCquODiuODmwrjgqrjg4rjg5vjg7zjg6sK44Kq44O844Ks44K644OgCuOCq+OCpuODkeODvA"
"rjgqvjg7Pjg4jjg7PljIXojI4K44Kt44Oz44K/44OeCuOCruODo+OCsOODnOODvOODqwrjgq/jgrnjgrMK44K"
"v44K944Ks44KtCuOCr+ODquODiOODquOCuQrjgq/jg7Pjg4vjg6rjg7PjgrDjgrkK44Kv44Oz44OLCuOCseOD"
"hOODnuODs+OCswrjgrPjg7Pjg4njg7zjg6AK44K144Ky44Oe44OzCuOCtuODvOODoeODswrjgrfjg4Pjgq/jg"
"rnjg4rjgqTjg7MK44K344On44K/44GK44GtCuOCueOCq+ODiOODrQrjgrnjgrHjg5kK44K544Kx44OZ5qSF5a"
"2QCuOCueODmuODq+ODngrjgrnjg6/jg4Pjg5Tjg7PjgrAK44K744OD44Kv44K5CuOCu+ODleODrArjgrvjg7P"
"jgrrjg6oK44K944OV44OI44O744Kq44Oz44O744OH44Oe44Oz44OJCuOCveODvOODl+ODqeODs+ODiQrjgr3j"
"g7zjg5flrKIK44OA44OD44OB44Ov44Kk44OVCuODgOODluODq+ODlOODvOOCuQrjg4Hjg7PjgrMK44OB44Oz4"
"4OB44OzCuODgeODs+ODnQrjg4fjgqPjg6vjg4kK44OH44Kj44O844OX44K544Ot44O844OICuODh+OCq+ODge"
"ODswrjg4fjg6rjg5Djg6rjg7zjg5jjg6vjgrkK44OH44Oq44OY44OrCuODiOODremhlArjg4rjg7Pjg5EK44O"
"O44O844OR44OzCuODj+ODoeaSruOCigrjg4/jg7zjg6zjg6AK44OQ44Kk44Ki44Kw44OpCuODkOOCreODpeOD"
"vOODoOODleOCp+ODqQrjg5HjgqTjgrrjg6oK44OR44Kk44OR44OzCuODkeODkea0uwrjg5Hjg7Pjg4Hjg6kK4"
"4OT44OD44OBCuODleOCo+OCueODiOODleOCoeODg+OCrwrjg5Xjgqfjg6kK44OV44Kn44Op44OB44KqCuODle"
"OCp+ODqeaKnOOBjQrjg5bjg6vjgrvjg6kK44Oa44OD44OG44Kj44Oz44KwCuODmuODi+ODkOODswrjg5vliKU"
"K44Oc44OG6IW5CuODneOCs+ODgeODswrjg53jg6vjg4HjgqoK44Oe44K544K/44O844OZ44O844K344On44Oz"
"CuODnuODs+OCswrjg6Djg6njg6Djg6kK44Ok44Oq44OB44OzCuODpOODquODnuODswrjg6njg5bjg4njg7zjg"
"6sK44Op44OW44ObCuODqeODluODm+ODhuODqwrjg6rjg5Xjg6wK44Os44Kk44OXCuODreODquOCs+ODswrkuI"
"DkurrvvKgK5Lit5Ye644GXCuS5mc+ACuS5seOCjOeJoeS4uQrkubHkuqQK5Lmz5oi/CuS5s+mmlgrkuoDnlLL"
"nuJvjgooK5LqA6aCtCuS6jOeptArkuoznqbTlkIzmmYIK5Luu5oCn5YyF6IyOCuS9k+S9jQrlgIvkurrmkq7l"
"vbEK5YKs55ygCuWFnOWQiOOCj+OBmwrlhaXoiLnmnKzmiYsK5YaG5YWJCuWHpuWlswrljIXojI4K5Y+j5YaF5"
"bCE57K+CuWPo+WGheeZuuWwhArllJDojYnlsYXojLboh7wK5ZaY44GO5aOwCuWbm+WNgeWFq+aJiwrlpKrjgo"
"LjgoLjgrPjgq0K5aer5aeL44KBCuWqmuiWrArlrZXjgb7jgZsK5a+d5Y+W44KJ44KMCuWvneWPluOCigrlr7/"
"mnKzmiYsK5bCE57K+CuWxjeWnpgrlt6jkubMK5beo5bC7CuW3qOaguQrluIbjgYvjgZHojLboh7wK5bqn5L2N"
"CuW8t+Wnpgrlvozog4zkvY0K5b6u5LmzCuW/jeOBs+WxheiMtuiHvArlv6vmpb3loJXjgaEK5oCn5LqkCuaAp"
"+WHpueQhgrmgKflpbTpmrcK5oCn5oSfCuaAp+aEn+ODnuODg+OCteODvOOCuArmgKfmhJ/luK8K5oCn5qyyCu"
"aAp+ihjOeCugrmhJvkuroK5oSb5pKrCuaEm+a2sgrmiJDkurrlkJHjgZEK5oiR5oWi5rGBCuaJi+OCs+OCrQr"
"miYvjg57jg7MK5omL5rerCuaKseOBjeWcsOiUtQrmj5rnvr3mnKzmiYsK5o+05LqkCuaPtOWKqeS6pOmamwrm"
"lL7lsL8K5pS+572u44OX44Os44KkCuaXqea8jwrmmYLpm6jojLboh7wK5pyI6KaL6Iy26Ie8CuacneWLg+OBo"
"QrmnJ3otbfjgaEK5p2+6JGJ5bSp44GXCuapn+e5lOiMtuiHvArmraPluLjkvY0K5rGB55S35YSqCuazoeWnqw"
"rmtJ7lhaXjgormnKzmiYsK5rer5LmxCua3q+ihjArmt6voqp4K5rer6Z2hCueGn+WlswrniIbkubMK542j5ae"
"mCueOieiIkOOCgQrnlJ/jg4/jg6EK55S35ai8CueXtOWlswrnmbrmg4UK55yf5oCn5YyF6IyOCuedoeWnpgrn"
"nb7kuLgK56iu5LuY44GRCueoruS7mOOBkeODl+ODrOOCuQrnqbTlhYTlvJ8K56uL44Gh44KT44G8Cuerpeiyn"
"grnrKDoiJ/mnKzmiYsK562G44GK44KN44GXCuetj+acrOaJiwrnspfjg4Hjg7MK57Sg6IKhCue0oOiCoSAK57"
"W25YCrCue2suS7o+acrOaJiwrnt4rnuJsK6IKJ5L6/5ZmoCuiDuOODgeODqQrohIfjgrPjgq0K6Ieq5oWwCui"
"PiumWgAron7vjga7miLjmuKHjgooK6KOP562LCuiyneWQiOOCj+OBmwrosqfkubMK6Laz44Kz44KtCui8quWn"
"pgrov5Hopqrnm7jlp6YK6YCG44Ki44OK44OrCumAhuODrOOCpOODlwrpgYXmvI8K6YeR546JCumZsOWUhwrpm"
"bDlmqIK6Zmw5qC4CumZsOavmwrpmbDojI4K6Zmw6YOoCumZtei+sQrpm4HjgYzpppYK6Zu744OeCumdkuWnpg"
"rpoZTlsIQK6aOf57OeCumjsuWwvwrpppblvJXjgY3mgYvmhZUK6aiO5LmX5L2NCum2r+OBruiwt+a4oeOCigr"
"pu4Tph5HmsLQK6buS44Ku44Oj44OrCu+8s++8reODl+ODrOOCpArvvoHvvp3vvoHvvp0KTlRSCk7jhJJSClTj"
"g5Djg4Pjgq8K44GI44Gj44GhCuOBiOOBo+OEjgrjgYjjgaPjhJgK44GK44Gh44KT44Gh44KTCuOBiuOEjuOCk"
"+OEjuOCkwrjgYrjhJjjgpPjhJjjgpMK44GV44GL44GV5qSL6bOlCuOBm+OBjeOCjOOBhOacrOaJiwrjgZvjga"
"PjgY/jgZkK44Gb44Gj44SR44GZCuOBoOOBhOOBl+OCheOBjeODm+ODvOODq+ODiQrjgaDjgYTjgZfjgoXjgY3"
"jg5vjg7zjhKbjg4kK44Gh44KT44GTCuOBoeOCk+OBoeOCkwrjgaHjgpPjgb0K44Gy44Go44KK44GI44Gj44Gh"
"CuOBsuOBqOOCiuOBiOOBo+OEjgrjgbLjgajjgorjgYjjgaPjhJgK44Ki44Kv44OhCuOCouOCr+OEqArjgqLjg"
"4Djg6vjg4jjg5Pjg4fjgqoK44Ki44OA44Sm44OI44OT44OH44KqCuOCouODiuODqwrjgqLjg4rjg6vjgrvjg4"
"Pjgq/jgrkK44Ki44OK44Or44OT44O844K6CuOCouODiuODq+ODl+ODqeOCsArjgqLjg4rjg6vmi6HlvLUK44K"
"i44OK44Or6ZaL55m6CuOCouODiuODq++8s++8pe+8uArjgqLjg4rjhKYK44Ki44OK44Sm44K744OD44Kv44K5"
"CuOCouODiuOEpuODk+ODvOOCugrjgqLjg4rjhKbjg5fjg6njgrAK44Ki44OK44Sm5ouh5by1CuOCouODiuOEp"
"umWi+eZugrjgqLjg4rjhKbvvLPvvKXvvLgK44Kk44Oh44Kv44OpCuOCpOODoeODvOOCuOODk+ODh+OCqgrjgq"
"TjhKjjgq/jg6kK44Kk44So44O844K444OT44OH44KqCuOCqOOCr+OCueOCv+OCt+ODvArjgqjjg4Pjg4EK44K"
"o44OtCuOCqOODreOBhArjgqjjg63lkIzkuroK44Ko44Ot5ZCM5Lq66KqMCuOCqOODreacrArjgqrjg4rjg5vj"
"g7zjg6sK44Kq44OK44Ob44O844SmCuOCquODvOOCrOOCuuODoArjgqrjg7zjgqzjgrrjhIoK44Kq44O844Ks4"
"4K644SZCuOCq+OCpuODkeODvArjgqvjg7Pjg4jjg7PljIXojI4K44Ku44Oj44Kw44Oc44O844OrCuOCruODo+"
"OCsOODnOODvOOEpgrjgrPjg7Pjg4njg7zjg6AK44Kz44Oz44OJ44O844SKCuOCs+ODs+ODieODvOOEmQrjgrb"
"jg7zjg6Hjg7MK44K244O844So44OzCuOCueOCq+ODiOODrQrjgrnjg5rjg6vjg54K44K544Oa44Sm44OeCuO"
"CueOEjOODiOODrQrjg4Djg5bjg6vjg5Tjg7zjgrkK44OA44OW44Sm44OU44O844K5CuODh+OCo+ODq+ODiQr"
"jg4fjgqPjhKbjg4kK44OH44Kr44OB44OzCuODh+ODquODkOODquODvOODmOODq+OCuQrjg4fjg6rjg5Djg6rj"
"g7zjg5jjhKbjgrkK44OH44Oq44OY44OrCuODh+ODquODmOOEpgrjg4fjhIzjg4Hjg7MK44OP44Oh5pKu44KKC"
"uODj+ODvOODrOODoArjg4/jg7zjg6zjhIoK44OP44O844Os44SZCuODj+OEqOaSruOCigrjg5Djgq3jg6Xjg7"
"zjg6Djg5Xjgqfjg6kK44OQ44Kt44Ol44O844SK44OV44Kn44OpCuODkOOCreODpeODvOOEmeODleOCp+ODqQr"
"jg5bjg6vjgrvjg6kK44OW44Sm44K744OpCuODneODq+ODgeOCqgrjg53jhKbjg4HjgqoK44Og44Op44Og44Op"
"CuODqeODluODieODvOODqwrjg6njg5bjg4njg7zjhKYK44Op44OW44Ob44OG44OrCuODqeODluODm+ODhuOEpg"
"rjhIrjg6njhIrjg6kK44SM44Km44OR44O8CuOEjOODs+ODiOODs+WMheiMjgrjhI7jgpPjgZMK44SO44KT44G"
"9CuOEjuOCk+OEjuOCkwrjhJLjg5Djg4Pjgq8K44SY44KT44GTCuOEmOOCk+OBvQrjhJjjgpPjhJjjgpMK44SZ"
"44Op44SZ44OpCuOEm+OBi+OEm+aki+mzpQrjhJzjgYvjhJzmpIvps6UK44Sd44GN44KM44GE5pys5omLCuOEn"
"eOBo+OBj+OBmQrjhJ3jgaPjhJHjgZkK44al44GN44KM44GE5pys5omLCuOGpeOBo+OBj+OBmQrjhqXjgaPjhJ"
"HjgZkK44ay44Kv44K544K/44K344O8CuOGsuODg+ODgQrjhrLjg60K44ay44Ot44GECuOGsuODreWQjOS6ugr"
"jhrLjg63lkIzkurroqowK44ay44Ot5pysCuWFnOWQiOOCj+OBmwrlhZzlkIjjgo/jhJ0K5YWc5ZCI44KP44al"
"CuWtleOBvuOBmwrlrZXjgb7jhJ0K5a2V44G+44alCuW/q+alveWgleOBoQrlv6vmpb3loJXjhI4K5b+r5qW95"
"aCV44SYCuacneWLg+OBoQrmnJ3li4PjhI4K5pyd5YuD44SYCuacnei1t+OBoQrmnJ3otbfjhI4K5pyd6LW344"
"SYCueUn+ODj+ODoQrnlJ/jg4/jhKgK56uL44Gh44KT44G8Cueri+OEjuOCk+OBvArnq4vjhJjjgpPjgbwK562"
"G44GK44KN44GXCuethuOBiuOEi+OBlwrosp3lkIjjgo/jgZsK6LKd5ZCI44KP44SdCuiyneWQiOOCj+OGpQrp"
"gIbjgqLjg4rjg6sK6YCG44Ki44OK44SmCum7kuOCruODo+ODqwrpu5Ljgq7jg6PjhKYK6IajCua3qwrlsLsK6"
"IKh6ZaTCuaAp+WZqArnsr7mtrIK57K+5a2QCuiCm+mWgArjgYLjgYIK44GB44GBCuOBieOBiQrjgYLjgYEK44"
"GB44GCCuOBguOAgeOBguOAgQrjgYLjgaPjgIHjgYLjgaMK44KT44CB44KTCuOCk+OBo+OAgeOCkwrjgYLjgYL"
"jgIHjgYLjgYIK44GC4oCm4oCm44GCCuOBgeKApuKApuOBgQrjgYXjgYUK44KL44KL44KLCuOBmOOCheOCiwrj"
"gaHjgoXjgosK44KT44KTCuOBiuOBiuOBigrjg7Pjg7Pjg7MK44Ki44Ki44KiCuOCoeOCoeOCoQrjgYbjgYbjg"
"YYK4oCm44Gh44KFCuKApuOBr+OBguKApgrjgarjgaoK44GC44CB44GCCuOBr+OBgeKApgrjgqTjgq/jgqTjgq"
"8K44G644KN44CBCuOBuuOCjeOCjQrjgpPjgbXjgYEK44Gv44GB44CBCuOBr+OBgeOAgeOBr+OBgeOAgQrjga/"
"jgYHjgIHjgpMK44GY44KF44G9CuOCjOOCi+KApgrjgozjgo3jgIHjgozjgo0K44O044Kh44Ku44OKCuOCquOD"
"nuODs+OCswrjgqrjg4Hjg7Pjg50K5oiR5oWi5rGBCuOCquODgeODs+ODgeODswrjg4Hjg7Pjg4Hjg7MK44GK4"
"4Gh44KT44GTCuOBiuOBoeOCk+OBvQrjgYrjg4Hjg7Pjg50K6ZuE44OB44Oz44OdCuOBoeOCk+OBkwrjgaHjgp"
"Pjgb0K44GK44Gh44KT44G9CuOCquODnuODs+OCswrjg57jg7PjgrMK44Ki44OM44K5CuOCouODiuODqwrjgrbjg7zjg6Hjg7M=";

SkipTransCfgPage::SkipTransCfgPage(toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("跳过翻译设置"));
    setContentsMargins(30, 15, 15, 0);

    // 创建中心部件和布局
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

    // skipH
    bool skipH = toml::find_or(m_projectConfig, "plugins", "SkipTrans", "skipH", false);
    ElaScrollPageArea* skipHArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* skipHLayout = new QHBoxLayout(skipHArea);
    ElaDoubleText* skipHText = new ElaDoubleText(tr("跳过 H 关键字"), 16,
        tr("关键字列表以 base64 编码形式存储在 SkipTrans.toml 中"), 10, "", skipHArea);
    skipHLayout->addWidget(skipHText);
    skipHLayout->addStretch();
    ElaToggleSwitch* skipHSwitch = new ElaToggleSwitch(skipHArea);
    skipHSwitch->setIsToggled(skipH);
    skipHLayout->addWidget(skipHSwitch);
    ElaPushButton* editHKeysButton = new ElaPushButton(tr("编辑"), skipHArea);
    editHKeysButton->setFixedWidth(80);
    skipHLayout->addWidget(editHKeysButton);
    mainLayout->addWidget(skipHArea);

	mainLayout->addSpacing(20);

    const std::string hKeysBase64 = toml::find_or(m_projectConfig, "plugins", "SkipTrans", "hKeys", hKeysBase64Default);
    m_hKeysWidget = new ElaWidget();
    ElaWidget* hKeysWidget = m_hKeysWidget;
    hKeysWidget->setWindowTitle(tr("编辑 H 关键词"));
    hKeysWidget->setWindowModality(Qt::ApplicationModal);
    hKeysWidget->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    hKeysWidget->resize(640, 720);
    QVBoxLayout* hKeysLayout = new QVBoxLayout(hKeysWidget);
    hKeysLayout->setContentsMargins(10, 0, 10, 10);
    hKeysLayout->setSpacing(8);
    ElaDoubleText* hKeysText = new ElaDoubleText(tr("H 关键词"), 16,
        tr("每行一个关键字，保存时会自动写回 base64"), 10, "", hKeysWidget);
    hKeysLayout->addWidget(hKeysText);
    ElaPlainTextEdit* hKeysEdit = new ElaPlainTextEdit(hKeysWidget);
    hKeysEdit->setMinimumHeight(420);
    QFont hKeysFont = hKeysEdit->font();
    hKeysFont.setPixelSize(14);
    hKeysEdit->setFont(hKeysFont);
    hKeysEdit->setPlainText(QString::fromUtf8(QByteArray::fromBase64(QByteArray::fromStdString(hKeysBase64))));
    hKeysEdit->moveCursor(QTextCursor::Start);
    hKeysLayout->addWidget(hKeysEdit);
    hKeysWidget->hide();
    connect(editHKeysButton, &ElaPushButton::clicked, this, [=]()
        {
            QWidget* mainWindow = window();
            if (mainWindow) {
                hKeysWidget->move(mainWindow->frameGeometry().center()
                    - hKeysWidget->rect().center());
            }
            hKeysWidget->show();
            hKeysWidget->raise();
            hKeysWidget->activateWindow();
        });

    // skipKeys
	toml::ordered_array skipKeysArr = toml::find_or_default<toml::ordered_array>(m_projectConfig, "plugins", "SkipTrans", "skipKeys");
	ElaText* skipKeysHelperText = new ElaText("skipKeys", 18, centerWidget);
	skipKeysHelperText->setWordWrap(false);
	ElaToolTip* skipKeysHelperTip = new ElaToolTip(skipKeysHelperText);
	skipKeysHelperTip->setToolTip(tr("语法与 retranslKeys 完全相同"));
	mainLayout->addWidget(skipKeysHelperText);
	ElaPlainTextEdit* skipKeysEdit = new ElaPlainTextEdit(centerWidget);
	skipKeysEdit->setMinimumHeight(330);

	QFont font = skipKeysEdit->font();
	font.setPixelSize(14);
	skipKeysEdit->setFont(font);
	installTreeSitterHighlighter(skipKeysEdit->document(), SyntaxLanguage::Toml);
	skipKeysEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "skipKeys", skipKeysArr}}})));
	skipKeysEdit->moveCursor(QTextCursor::Start);
	mainLayout->addWidget(skipKeysEdit);


    m_applyFunc = [=]
        {
            insertToml(m_projectConfig, "plugins.SkipTrans.skipH", skipHSwitch->getIsToggled());
            const QByteArray hKeysBase64Data = hKeysEdit->toPlainText().toUtf8().toBase64();
            insertToml(m_projectConfig, "plugins.SkipTrans.hKeys",
                std::string(hKeysBase64Data.constData(), hKeysBase64Data.size()));

			try {
				toml::ordered_value newSkipKeysTbl = toml::parse_str<toml::ordered_type_config>(skipKeysEdit->toPlainText().toStdString());
				auto& newSkipKeysArr = newSkipKeysTbl["skipKeys"];
				if (newSkipKeysArr.is_array()) {
					insertToml(m_projectConfig, "plugins.SkipTrans.skipKeys", newSkipKeysArr);
				}
				else {
					insertToml(m_projectConfig, "plugins.SkipTrans.skipKeys", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析错误"), tr("skipKeys 不符合 toml 规范"), 3000);
			}
        };

    mainLayout->addStretch();

    centerWidget->setWindowTitle(tr("跳过翻译设置"));
    addCentralWidget(centerWidget, true, false, 0);
}

SkipTransCfgPage::~SkipTransCfgPage()
{
    delete m_hKeysWidget;
}
