// 声明关于本机和网络检测辅助页的构建、刷新与对象清理接口。
#pragma once

void build_boot_info_page();
bool update_boot_info_page();
void build_network_diag_page();
bool update_network_diag_page();
void clear_aux_page_object_refs();
