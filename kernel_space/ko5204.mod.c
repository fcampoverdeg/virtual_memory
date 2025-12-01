#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xa61fd7aa, "__check_object_size" },
	{ 0x092a35a2, "_copy_from_user" },
	{ 0x9479a1e8, "strnlen" },
	{ 0xd7a59a65, "vmalloc_noprof" },
	{ 0x27683a56, "memset" },
	{ 0xbd03ed67, "vmemmap_base" },
	{ 0x39240082, "vmalloc_to_page" },
	{ 0xf1de9e85, "vfree" },
	{ 0xd710adbf, "__kmalloc_large_noprof" },
	{ 0xbd03ed67, "page_offset_base" },
	{ 0xcb8b6ec6, "kfree" },
	{ 0xbd03ed67, "phys_base" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0xe54e0a6b, "__fortify_panic" },
	{ 0xb9e81daf, "proc_remove" },
	{ 0xd272d446, "__fentry__" },
	{ 0xf8d7ac5e, "proc_create" },
	{ 0xe8213e80, "_printk" },
	{ 0x70eca2ca, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xd272d446,
	0xa61fd7aa,
	0x092a35a2,
	0x9479a1e8,
	0xd7a59a65,
	0x27683a56,
	0xbd03ed67,
	0x39240082,
	0xf1de9e85,
	0xd710adbf,
	0xbd03ed67,
	0xcb8b6ec6,
	0xbd03ed67,
	0xd272d446,
	0x90a48d82,
	0xe54e0a6b,
	0xb9e81daf,
	0xd272d446,
	0xf8d7ac5e,
	0xe8213e80,
	0x70eca2ca,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"__x86_return_thunk\0"
	"__check_object_size\0"
	"_copy_from_user\0"
	"strnlen\0"
	"vmalloc_noprof\0"
	"memset\0"
	"vmemmap_base\0"
	"vmalloc_to_page\0"
	"vfree\0"
	"__kmalloc_large_noprof\0"
	"page_offset_base\0"
	"kfree\0"
	"phys_base\0"
	"__stack_chk_fail\0"
	"__ubsan_handle_out_of_bounds\0"
	"__fortify_panic\0"
	"proc_remove\0"
	"__fentry__\0"
	"proc_create\0"
	"_printk\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "0BA6B6176218E77B65B0AA8");
