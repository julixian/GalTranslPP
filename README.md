# GalTransl++

![GalTransl++ GUI](img/GalTranslPP.png?raw=true)
![GalTransl++ GUI En](img/GalTranslPP_en.png?raw=true)

**GalTransl++** 是继承了 [GalTransl](https://github.com/GalTransl/GalTransl)  `以项目为本`的主要理念及架构，凝练其两年间积累的精华部分，同时吸收了大量Gal补丁作者经验而进行优化的，轻量透明的、拥有高度且方便的扩展能力的翻译核心。

**GalTransl++ GUI** 是包装了GalTransl++核心的，以 `在GUI下尽可能保持高度自定义` 为目标的，Fluent UI风格的交互界面，也是本项目的重点开发对象。

## ✨ 特性

本项目在继承了GalTransl基本功能的基础上，包括但不限于对以下模块进行了优化：

* 更好的单文件分割缓存命中
* 优先标点的换行修复
* 可选正则形式的，高度自定义的译前译后字典和明确的优先级
* 高度自定义的Epub提取
* 有效的API额度耗尽检测
* 卡片弹出式的完成提示 (仅GUI)
* 更好的字典未使用检测
* 可自定义的符号检测
* 单独生成用以检查的预处理结果
* 统合生成的翻译问题概览
* 速度更快的Rebuild
* 更加方便的提示词自定义
* 更清晰的字典使用设定
* 重翻时附带已知问题
* 可自定义的问题比较对象
* 对 条件判断/文本处理/文件格式处理 的自定义 Lua/python 语言支持

![notification](img/notification.png?raw=true)

## 📖 流程说明

### GalTransl++ 翻译流程与替换型字典介绍

对于熟悉GalTransl的人来说，过渡到GalTransl++ CLI版本可谓易如反掌，所以接下来主要讲GUI。

这里还是再稍微谈一下翻译流程吧(默认你已经读过GalTransl的使用说明，翻译接口什么的就略过了)。

GalTransl++无论处理哪种文件格式，最后都是统一化为json来读取。

每个json文件是一个对象列表，每个对象代表一个『句子』，这是喂给AI进行翻译的基本单位。

读取json时主要关注两个键，分别为 `name` 和 `message`，不同的翻译模式(`transEngine`)就是对这两个键进行各种不同的处理：


#### 翻译模式 (transEngine)

* **`# ForGalJson`**: 实际翻译模式，向AI输入json格式的句子(包含`name`和`message`)并要求AI以json格式回复，程序将解析返回的Json。容易出现傻瓜引号解析错误的问题，建议优先使用ForGalTsv。
* **`# ForGalTsv`**: 实际翻译模式，向AI输入TSV格式的句子(包含`name`和`message`)并要求AI以TSV格式回复，程序将解析返回的TSV，可能比ForGalJson模式更省token。
* **`# ForNovelTsv`**: 实际翻译模式，和 `ForGalTsv` 的区别主要是变动提示词，向AI输入和解析的时候都不带`name`键。
* **`# Sakura`**: 实际翻译模式，向AI输入自然语言形式的句子(包含`name`和`message`)，由于Sakura是翻译特化模型，不必要求即会返回同样形式的的句子，程序解析返回的自然语言。
* **`# DumpName`**: 提取所有的 `name` 键，在项目文件夹下生成 `人名替换表.toml` 以供统一替换人名(已有则仅更新)。
* **`# NameTrans`**: 翻译人名表(如果没有则先Dump)。
* **`# GenDict`**: 借助AI自动生成术语表，保存在项目文件夹下的 `项目GPT字典-生成.toml` 中。
* **`# Rebuild`**: 即使 `retranslKey` 命中也不会重翻，只根据缓存重建结果。
* **`# ShowNormal`**: 保存预处理后的内容及句子到项目文件夹下带 `show_normal` 字段的文件夹中，如Epub格式下可生成预处理后的html/xhtml文件以及生成的json，可用于检查和排错。

### 缓存机制

在`Rebuild`中所提到的缓存，是指翻译过后留存在项目文件夹下，`trans_cache`文件夹中的json文件，其中按顺序存储了每个文件对应的序列号，原文和译文等信息。所有的实际翻译模式都会先读取缓存，然后只挑选出缓存中还没有的原文进行翻译。

> **⚠️ 特别注意**：在使用单文件分割功能的情况下，由于缓存命中结合了上下文，所以当你改变文件本身，或者分割数/分割方式时，会有一部分无关的句子不能命中缓存。理论上文件切的越碎，最终分割出的文件份数比最大线程数超过的更多，则不能命中缓存的句子越多。GalTransl++会尽可能在这种情况下保证原有缓存的命中(你也可以调整`分割缓存查找距离`来控制缓存查找)，不过如果希望达到更好的缓存命中，最好还是不改变分割方式和分割数。为此也可以使用 `ShowNormal` 模式观察切割后的文件。

`retranslKey`指的是重翻关键字。

GalTransl++会在翻译时自动分析翻译时常见问题，并将问题输出到缓存中。

一般情况下，如果 `problems` 中包含设定的`retranslKey`(当然也可以是别的什么条件)，则即使在实际翻译模式下命中缓存，这个句子依然会被重翻。所以如果只想重建缓存，要么得删除所有`retranslKey`，要么使用 `Rebuild` 模式忽略翻译。

### 字典系统

GalTransl++的字典分为 **译前字典**，**GPT字典**，**译后字典** 三大类。每类字典都有 **通用** 和 **项目** 两种。

顾名思义，通用字典可以被所有项目所见，项目字典只能被当前项目所见。

具体一个项目要用哪些字典，可以在项目的 `翻译设置->字典设置` 中找到对应的选项进行选择。


这三类字典中，译前和译后字典为 **替换型字典**，GPT字典为 **提示型字典**。

即译前和译后字典执行的是搜索替换，而GPT字典的作用仅在于当原文中出现字典里的词时，将该条字典作为附加部分一并喂给AI以规范翻译，那至于AI想不想用，用成什么样，就不是程序能管得到的了。


#### GUI中的字典处理

* **文件对应关系**
  * 通用字典可以有多个，而项目字典和人名表每个项目各只有一个。
  * GUI会读取项目文件夹下 `人名替换表.toml` 来作为 **人名表** 中的数据。
  * 读取 `项目字典_译前.toml` 来作为 **项目译前字典** 中的数据。
  * 读取 `项目GPT字典.toml` 和 `项目GPT字典-生成.toml` 并合并其中的数据来作为 **项目GPT字典** 中的数据。
  * 读取 `项目字典_译后.toml` 来作为 **项目译后字典** 中的数据。

* **编辑模式与保存逻辑**
  * 人名表和字典都分别有 **纯文本模式** 和 **表模式**，具体在翻译时用哪个模式的数据会在你按开始翻译按钮时决定。
  * 例如，你在按开始翻译按钮时，人名表是以表模式显示的，则会先将 **人名表(表模式)** 中的数据保存到 `人名替换表.toml` 中，然后再执行翻译。如果在纯文本模式下没有按 toml 格式来编辑，翻译时肯定会报错。
  * 按 **刷新** 将会重新从项目文件夹中的 toml 文件读取数据。如果你在GUI中还有修改了没有保存的数据，请务必先确认备份情况再刷新。
  * 按 **保存** 会在保存的同时刷新另一模式的数据。比如在纯文本模式中编辑后按下保存，则此时表模式也会更新刚刚编辑过的内容。另外，保存 **项目GPT字典** 时会 **删除**  `项目GPT字典-生成.toml` 以防止数据重复，请务必注意。最好的实践是生成后立即保存，不要编辑`项目GPT字典-生成.toml`。

### 一个常见的翻译流程

1、  新建项目 -> 输入项目名。  
2、  在新建的项目文件夹中的`gt_input`文件夹中放入待翻译的文件。  
3、  填入API和key。  
4、  使用 `GenDict` 自动生成术语表。  
5、  调整术语表，根据需求修改字典并选择要使用的字典。  
6、  如果文件支持提取name，则可 `DumpName` 并编辑人名表，也可 `NameTrans` 翻译人名表。  
7、  选用合适的实际翻译模式进行翻译。  
8、  查看问题。  
9、  根据问题选择编辑`retranslKey`/`skipProblems`重建/重翻/修改缓存/...。  
10、 重翻 / 重建。  
11、 在`gt_output`中查收结果。  

### 缓存文件结构

GalTransl++的缓存中可能包含如下键:

* `index`: 索引顺序，一般不重要。
* `name`: 人名，展示的是预处理后的人名(相当于pre_processed_name)，如果没有则为空。
* `names`: 复数人名，仅见于部分 VNT 提出的文本中。
* `name_preview`: 将输出的译后人名。
* `names_preview`: 同上
* `original_text`: 原文对照，与原文没有任何区别。
* `pre_processed_text`: 经过一系列预处理后，即将执行AI翻译前时句子的样子。
* `pre_translated_text`: AI返回的，未经过任何后处理的句子的样子。
* `other_info`: 其它附加信息。
* `problems`: 在自动化找错中找出的问题。
* `translated_by`: 翻译此句子所用的 apikey 的设定模型。
* `translated_preview`: 经过所有后处理之后，将输出的 message。

### 替换型字典语法

* **译前字典** 会搜索并替换 `original_text` 以输出 `pre_processed_text` 提供给AI。
* **译后字典** 会搜索并替换 `pre_translated_text` 以供 `translated_preview` 最终输出。

**条件对象** 是指条件正则要作用于的文本，可以是 `name`, `orig_text`, `preproc_text`, `pretrans_text` 中的任意一个。

当 **启用正则** 为 `true` 时，原文和译文将被视为正则表达式进行替换，优先级越高的字典越先执行。

## ⚙️ 处理与翻译顺序

1、  执行 dprerun 阶段的插件。  
2、  可选: 对 `name` 执行译前字典替换 (人名表收集的也是这一步后的 `name`)。  
3、  将 `message` 中的换行统一替换为 `<br>`。  
4、  将 `message` 中的制表符统一替换为 `<tab>`。  
5、  可选: 对 `message` 执行译前字典替换。  
6、  执行 prerun 阶段的插件。  
7、  **AI翻译**。  
8、  解析 **翻译失败(Failed to translate)** 和 **GPPCProblem**，如果翻译失败则不分析其它问题。  
9、  执行 postrun 阶段的插件。  
10、  可选: 对 `message` 执行译后字典替换。  
11、  将 `message` 中的 `<tab>` 统一替换回制表符。  
12、 将 `message` 中的换行从 `<br>` 换回原符号。  
13、 可选: 对 `name` 执行GPT字典替换 (此时GPT字典将被临时视为替换型字典)。  
14、 对 `name` 执行人名表替换 (人名表译名为空则忽略)。  
15、 可选: 对 `name` 执行译后字典替换。  
16、 执行 dpostrun 阶段的插件。  
17、 问题分析。  

GPPCProblem: 通过 Prompt 让模型在翻译可疑句子时在译文结果前加上 `(GPPCProblem:xxx)` 格式的句子来让模型自己添加问题  

> **正则引擎说明**
>
> 虽然GalTransl++的核心为C++编写，但由于正则引擎采用pcre2且默认使用unicode参数编译正则，所以可自定义编辑的所有正则依然默认是以字符为单位的。比如即使emoji表情'😪' 在UTF-8中占用4个字节，在正则中依然是以一个`.`来匹配的。
>
> 同时也支持pcre2所支持的所有扩展正则语法，如可以使用`\p{P}`来匹配任意标点符号，`\p{Hangul}`来匹配任意韩文字符等。

## 其它翻译说明

<details>

<summary>

### 问题分析 语法示例:

</summary>

retranslKeys 语法示例

```
# 正则表达式列表，如果句子缓存中的某条 problem 能被以下任一正则 search 通过，则进行重翻
# 如果想对指定原文/译文进行重翻，请通过内联表(数组)指定 conditionTarget 和 conditionReg
# 也可以指定 conditionScript 和 conditionFunc 来外接 lua/py 脚本(conditionFunc 接收 Sentence，返回bool)
# <PROJECT_DIR>为代表当前路径字符串的宏
# conditionTarget 加前缀 prev_ 或 next_ 可表示 前/后 句，如 prev_prev_orig_text 表示上上句原文，如果没有则条件失败
retranslKeys = [
  #[{ conditionScript='<PROJECT_DIR>/Lua/MySampleTextPlugin.lua',conditionFunc='funcName'},
  #{ conditionScript='<PROJECT_DIR>/Python/MySampleTextPlugin.py',conditionFunc='funcName'}],
  #"残留日文",
  "翻译失败", # 等效于 [{ conditionTarget = 'problems', conditionReg = '翻译失败' }]
]
```

skipProblems 语法示例

```
# 正则表达式列表，如果一条 problem 能被以下任一正则 search 通过，则不加入 problems 列表
# 如果想忽略指定原文/译文的指定问题，请通过内联表(数组)指定 conditionTarget 和 conditionReg
# 并在表数组第一项填入要忽略的 problem (同样也是正则表达式)
skipProblems = [
  # "^引入拉丁字母: Live$"  # 不加任何条件
  # 如果 原文中包含'モチモチ'且译文中包含'Q弹'，则忽略此句子中所有能被 '引入拉丁字母' search 通过的 problem
  [ '引入拉丁字母', { conditionTarget = 'preproc_text', conditionReg = 'モチモチ'},
    { conditionTarget = 'trans_preview', conditionReg = 'Q弹'}],
]
```

问题比较对象设定 语法示例:

```
overwriteCompareObj = [
    { check = 'trans_preview', problemKey = '残留日文' },
    { base = 'preproc_text', check = 'trans_preview', problemKey = '标点错漏' },
    { base = 'preproc_text', check = 'trans_preview', problemKey = '引入拉丁字母' },
    { base = 'preproc_text', check = 'trans_preview', problemKey = '引入韩文' },
    { base = 'orig_text', check = 'trans_preview', problemKey = '丢失换行' },
    { base = 'orig_text', check = 'trans_preview', problemKey = '多加换行' },
    { base = 'preproc_text', check = 'trans_preview', problemKey = '字典未使用' },
    { base = 'preproc_text', check = 'trans_preview', problemKey = '语言不通' }
]
```
如 `{ problemKey = '比原文长严格', base = 'orig_text', check = 'trans_preview' }`， 

意思是当 trans\_preview 比 orig\_text 严格长时，在 problem 中留下对应的问题。
</details>

<details>

<summary>

### Epub 提取与正则自定义

</summary>

鉴于Epub的多样性以及GalTransl++以项目为本的理念，GalTransl++的Epub提取并不像其它翻译器一样有固定的解析/拼装模式。

GalTransl++将仅使用GUMBO库遍历Epub中的HTML/XHTML文件
(epub文件本身只是一个zip包，你可以使用任何解压软件打开它并浏览其中的文件)，

每个文本标签提取出的字符串都将作为一个msg。

但仅仅这样会有一些明显的问题，如下面这个句子:
```
<p class="class_s2C-0">「モテる男は<ruby><rb>辛</rb><rt>つら</rt></ruby>いね」</p>
```
它会被拆解为 `「モテる男は` `辛` `つら` `いね」` 四个部分，作为4个msg喂给AI，这显然不是我们想要的。如果使用固定的html解析方法/固定的正则组进行提取，要么会丢失信息，要么无法兼顾所有情况，总之自定义程度极低，最后输出的效果理想与否完全取决于程序的解析与拼装模式是否正好符合用户的预期。所以GalTransl++选择让用户根据自己的项目自行利用正则构建解析模式，这显然增长了一些门槛，不过作为回报大幅提升了自由度。

Epub正则设置依然使用toml语法解析配置，并支持两种正则写法: **一般正则**和**回调正则**。


#### 一般正则

一般正则执行简单的匹配替换，例如使用下面的预处理正则，
```
[[plugins.Epub.preprocRegex]]
org = '<ruby><rb>(.+?)</rb><rt>(.+?)</rt></ruby>'
rep = '[$2/$1]'
```
则上面的句子会被替换为:
```
<p class="class_s2C-0">「モテる男は[つら/辛]いね」</p>
```
之后再遍历到此处时，「モテる男は\[つら/辛\]いね」 将被作为一个完整的句子被提取出来。如果之后搭配如下后处理正则，
```
[[plugins.Epub.postprocRegex]]
org = '\[([^/\[\]]+?)/([^/\[\]]+?)\]'
rep = '<ruby><rb>$2</rb><rt>$1</rt></ruby>'
```
则可在理想位置还原振假名效果(可以修改提示词告诉AI注音格式\[振假名/基本文本\]及保留要求来达到更好的保留效果)。当然如果只想在原文中保留振假名而在译文中删去，那也很简单，直接在译前字典中再写一个去注音字典即可。其它很多自定义需求也都可借此满足。

#### 回调正则

不过如上正则仍有一些缺陷，比如这两句话:
```
<p class="class_s2C-0">「うっ、レナ<span class="class_s2R">!?</span>」</p>
<p class="class_s2C-0">　二人の女の子が火花を散らす。どちらからの好意も嬉しくて、ついつい甘んじてし<span id="page_8"/>まう。</p>
```
假如我不想一个一个文件的看标签写正则，而是想要直接『删除所有`<p></p>`标签内的其余标签并保留非标签部分』来快速过滤的话，面对不擅长处理嵌套的简单正则，显然我们很难写出这样的正则/正则组来处理这个问题，那么此时就需要使用回调正则。

注: 2.2.4版本之后，想达到这个目的可以简化为一条正则
```
[[plugins.Epub.postprocRegex]]
org = '((?i)(?:<p\b[^>]*>|\G(?!\A))[^<]*\K<(?!/p>)[^>]*>)'
rep = ''
```

我们可以编写如下回调正则，
```
[[plugins.Epub.preprocRegex]]
org = '(<p[^>/]*>)(.*?)(</p>)'
callback = [ { group = 2, org = '<[^>]*>', rep = '' } ]
```
则这两句话会被替换为
```
<p class="class_s2C-0">「うっ、レナ!?」</p>
<p class="class_s2C-0">　二人の女の子が火花を散らす。どちらからの好意も嬉しくて、ついつい甘んじてしまう。</p>
```

##### 回调正则处理流程

回调正则的语法是，处理每个匹配项中的所有捕获组，对每个捕获组使用其对应`group`的回调正则/正则组进行替换，最后将所有捕获组按顺序合并作为这个匹配项的替换字符串。

采用第一句话作为例子，第一句话 `<p class="class_s2C-0">「うっ、レナ<span class="class_s2R">!?</span>」</p>` 整句被正则 `'(<p[^>/]*>)(.*?)(</p>)'` 匹配到，则这一整句作为一个匹配项。在这个匹配项中，根据捕获组又可以分为三组(没有被分组的部分则直接忽略)：

*   **第一组:** `<p class="class_s2C-0">`
*   **第二组:** `「うっ、レナ<span class="class_s2R">!?</span>」`
*   **第三组:** `</p>`

对于第一组文本，程序会寻找所有 `group = 1` 的回调正则，并依次执行替换。显然上面的回调正则并没有 `group = 1` 的正则，则忽略替换，第一组原样输出，现在的替换字符串暂定为: `<p class="class_s2C-0">`

对于第二组文本，程序找到了 `group = 2` 的正则，则对第二组文本使用 `org = '<[^>]*>', rep = ''` 进行替换，替换后得到 `「うっ、レナ!?」` ，则现在的替换字符串暂定为 `<p class="class_s2C-0">「うっ、レナ!?」`

第三组文本与第一组文本同理，则最终的替换字符串为 `<p class="class_s2C-0">「うっ、レナ!?」</p>` ，与预期相符。
</details>

<details>

<summary>

### Python 模块使用及 GPU 加速

</summary>
由于许多有关深度学习的库(如分词器、PDF提取器等)只有python能方便的调用，本程序在发行时会默认附带小型的嵌入式Python环境，你无需手动下载。

当遇到需要使用Python库的情况时(如**翻译PDF**或**使用依赖Python的分词器**)，程序会自动为嵌入式环境下载对应的库。

但可能在下载之后需要**重启程序**以重新加载Python解释器。请留意日志输出窗口的提示，避免造成程序卡死或崩溃。

然而在不启用 GPU加速 的情况下使用如 `spaCy最好的trf模型` 或 `Stanza` 进行全文分词的速度是比较灾难性的，如果想启用 GPU 加速，请跟随以下教程。

**请确保自己有一定的动手和思考能力 && 一块还不错的显卡！**

#### 为 `Stanza` 启用 GPU加速

- 1、 首先确保你安装了适配你显卡的 **最新的N卡驱动**，然后去[NVIDIA CUDA Toolkit官网](https://developer.nvidia.com/cuda-toolkit-archive) **选择合适的 CUDA Toolkit 版本**, 下载 CUDA Toolkit Installer 并安装。
不知道怎么选 CUDA 版本的可以在 cmd 里运行
```cmd
nvidia-smi
```
以获取当前驱动最高支持的 CUDA 版本(之所以要先更新驱动是因为不同的驱动版本所能支持的 CUDA 版本可能也会有变化)。
- 2、 访问[PyTorch官网](https://pytorch.org/get-started/locally/)，选择合适的 CUDA 版本获取安装命令。
忘了自己电脑上装的 CUDA 是什么版本的可以运行
```cmd
nvcc --version
```
以获取当前系统的 CUDA 版本，一般都向后兼容，选哪个问题都不大。
- 3、 为嵌入式环境安装 PyTorch，注意启动的必须是 **嵌入式环境中的 Python(之后的操作默认均在此环境中进行)**。
默认目录在 `BaseConfig\python-3.12.10-embed-amd64`，请在此目录下打开 cmd 或在 cmd 每次执行命令时输入此环境的 python.exe 的**绝对路径**以避免与你可能安装过的 python 混淆(pip同理，必须 env/python.exe -m pip...)。
- 4、 比如官网给我的命令是 `pip3 install torch torchvision --index-url https://download.pytorch.org/whl/cu129`，那我就可以在如上目录中打开 cmd (直接在路径栏输入 cmd 后回车)并运行 `python -m pip install torch torchvision --index-url https://download.pytorch.org/whl/cu129`。
注意: 如果你已经安装了torch，最好先卸载它：`python -m pip uninstall torch`
- 5、 重装 Stanza，`python -m pip uninstall stanza`  `python -m pip install stanza`
- 6、 尝试运行 `BaseConfig\pyScripts\check_stanza_gpu.py`，如果提示成功，则代表所有配置均已就绪。
- 7、 此时打开 `BaseConfig\pyScripts\tokenizer_stanza.py` 文件，将 `self.nlp = stanza.Pipeline(lang=model_name, processors='tokenize,pos,ner', use_gpu=False, verbose=False)` 中的 `use_gpu` 参数改为 `True`，即可为 Stanza 启用 GPU加速。

#### 为 `spaCy` 启用 GPU加速

- 1、 同 Stanza 第一条 至 第四条。
- 2、 在 **嵌入式 Python环境(详见 Stanza第三条)中** 重新安装 spacy。`python -m pip uninstall spacy`，并确保此环境中没有安装 `cupy`。
- 3、 根据自己的 CUDA 版本(查看 Stanza 第二条以查看如何获取 CUDA 版本)，安装 `cupy` 的特定版本，如 `cupy-cuda13x`: `python -m pip install cupy-cuda13x`，然后再把 spacy 装回来 `python -m pip install spacy`。
- 4、 尝试运行 `BaseConfig\pyScripts\check_spacy_gpu.py`，如果提示成功，则代表所有配置均已就绪。
- 5、 此时打开 `BaseConfig\pyScripts\tokenizer_spacy.py` 文件，将 `#spacy.require_gpu()` 的#注释去掉，即可为 spaCy 启用 GPU加速。

</details>

<details>

<summary>

### Lua/Python 内嵌

</summary>
这两种语言在程序内均为热更新(只要没有被其它任务占用)，更改后重新运行即可看到新的结果。

可以非常方便的编写自定义的 **文件解析/文本处理/任务结束后处理** 等脚本。

具体的代码示例详见 `Example/Lua` 及 `Example/Python`，工具函数/类函数的签名需要你自行翻阅一下 `LuaManager.cpp` 和 `PythonManager.cpp`。

</details>

## 其它程序说明

<details>

<summary>

### 自定义主页Popular卡片

</summary>
具体示例详见 (Example/)BaseConfig/globalConfig.toml 中的 [[popularCards]] 数组。

卡片数组至少六个，不足六个的将使用程序默认的卡片补齐，最高不限数量。

当 `fromLocal` 为 `true` 且 `cardPixmap` 为空或不存在时，将自动获取本地文件的图标进行填充。

当 `fromLocal` 为 `true` 时，卡片按钮文本将变为『启动』，同时自动将 `pathOrUrl` 中的路径进行 fromLocal 转化。

不过不会改变工作目录，如果遇到一些没有给自己设定工作目录为『程序所在目录』的程序，可能就会出问题。

这种情况可以写一个中转脚本，
```cmd
chcp 65001
cd /d %~dp0
start /b 要运行文件的文件名
```
然后启动这个中转脚本就可以了。

**注意:** 不要在程序运行的时候修改 `globalConfig.toml`，会被刷掉，请关闭程序后再进行修改。
</details>

<details>

<summary>

### 可在新窗口打开的标签页

</summary>

GUI界面所有的以标签页形式呈现的字典、人名表、提示词等均可以拖出来在新窗口中打开，如下图所示:

![tabView](img/tabView.png?raw=true)
</details>

<details>

<summary>

### 快捷清屏

</summary>

可以使用快捷键(默认为 Ctrl+L)清除当前日志页面的输出。  
更改快捷键请手动修改 BaseConfig/globalConfig.toml 中 `clearLogShortcut` 所存储的值。  

</details>


## ⚙️ 编译指南

详见[how-to-build.md](how-to-build.md)

## 🤝 贡献指南

GalTransl++在文件支持和插件支持上仍处于起步阶段，也不排除有其它优化的思路或需要修改的bug，如果有疑问或建议，欢迎提出 issue 或 PR。接下来主要说一下如何添加文件处理器/插件。

### 添加文件处理器 (Translator)

如你所见，GalTransl++的接口也十分简单。

由于所有的文件处理器需直接/间接继承自 `ITranslator`，如无特殊情况，一般直接继承 `NormalJsonTranslator` 即可。

这样只需要将相应文件提取为json，并重设提取文件夹为 `NormalJsonTranlator` 的 `inputDir` 文件夹，

重设期望获取译后json的文件夹为其 `outputDir` 文件夹即可。

每当其翻译完一个文件时(如果有单文件分割则只在文件合并后)，其会调用成员变量中的函数对象 `m_onFileProcessed`(在非 pythonTranslator 时线程安全)，

可以借此来判断文件的写回时机，具体示例可见 `EpubTranslator`。

需要注册的工厂函数为 `createTranslator`。


### 添加插件处理器 (Plugin)

插件只需满足 `PPlugin` 约束即可。

* **对于 dprerun/prerun 阶段**：如果是非过滤型插件，原则上只允许修改 `pre_processed_text`。如果是过滤型插件，可以将 `complete` 置为 `true`，并负责填充 `pre_translated_text`。如有需要，也可以将 `notAnalyzeProblem` 置为 `true` 来阻止对此句的问题分析。
* **对于 postrun/dpostrun 阶段**：原则上只允许修改 `translated_preview`。

任意插件均可在 `other_info` 中插入键以在缓存中附带信息。

具体示例可见 `TextLinebreakFix` 和 `TextFull2Half`。

需要注册的工厂函数为 `registerPlugins`。


### 其它注意事项

由于我的开发环境（特别是构建）基本绑定 windows 系统，我自己也没有 Linux/Mac 设备，所以即使在代码本身中使用的 winapi 数量很少也很好替换，

跨平台的事我自己也是不会主动考虑的。

另外由于我所使用的环境较新，也可能会有一些比较罕见的问题。

~~目前已知项目依赖 `mecab:x64-windows` 在VS2026(工具集 v145)下不过编，但是VS2022(工具集 v143)能过，安装依赖可能需要切回VS2022。~~(此问题已修复)  
由于 MSVC 的 [bug](https://developercommunity.visualstudio.com/t/C-20-Modules-unable-to-import-std-usin/11075026?ref=native&refTime=1777570126918&refUserId=7c1b7679-4205-6f72-b1be-bbf96d663676)，目前本项目无法使用 VS2026 Insiders 构建。
