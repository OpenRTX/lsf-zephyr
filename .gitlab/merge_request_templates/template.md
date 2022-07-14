//所有带双斜杠符号（C 语言注释符）的文本都应该要删除(<font color="#DD0000">包括本行</font><br />)，<font color="#DD0000">每节的title都必须保留</font><br />

## Checklist

* [ ] 已修改对应的 issue的状态
* [ ] MR 的Milestones、Lable都已正确设置
* [ ] MR 的 Title、Commit message 足够的简洁且精准
* [ ] 足够的信息帮助 reviewer 理解 feature 的大致实现或者 bug 的相关细节
* [ ] Ci pipeline 已运行且通过
* [ ] 等待 merge

## Related

//如果没有对应的属性，可删除该行属性

* [工单]: //如果这个 MR 关联一个工单（包括需求和bug工单等），这里填写对应的 <font color="#DD0000">工单链接</font><br />
* [Sub_MR]: //如果这个 MR 关联别的子仓库的MR操作,这里填写相关的子 MR链接
* [Repository]: //如果这个 MR 依赖别的的另一个仓库的新commit，这里填写相关仓库 commit链接
* [Backport]: //如果这是一个 backport MR，这里填写原始 MR链接
* [other]: //如果还有别的相关的链接或者文档，这里填写对应的 链接

## Release notes (Mandatory)

//每一个 MR 都应该强制编写 Release notes，特别是 feature 类 MR 在 commit message 中无法体现出更多的信息。一些无关紧要的注释拼写 fix 等可自行斟酌是否需要此章节

* [Ai/noise cancellation] Changed/fixed/updated CSK4002NC lib/respak
* [System/os] Added support of cskxxx SMP in FreeRTOS
* [Peripheral/driver] Fixed xxx

## Breaking change notes

//如果没有 breaking change，本节可为空

* [driver/uart] `uart_transfer_data()` 函数增加多一个参数，用来支持 polling 传输与 DMA 传输
