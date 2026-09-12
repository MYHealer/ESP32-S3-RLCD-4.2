// 声明跨模块复用的 ASCII 首尾空白裁剪策略。
#pragma once

// 使用 C isspace 规则，包含空格、Tab、回车、换行、垂直制表和换页。
void trim_ascii_whitespace(char *text);

// 只裁剪面向单行配置的空格、Tab、回车和换行。
void trim_ascii_line_whitespace(char *text);
