/*
 * The MIT License
 *
 * Copyright (c) 2011 Shuhei Tanuma
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "php_phpskirt.h"

zend_class_entry *phpskirt_class_entry;

void php_phpskirt_init(TSRMLS_D);

typedef enum
{
	PHPSKIRT_RENDER_HTML,
	PHPSKIRT_RENDER_TOC
} PHPSkirtRendererType;


ZEND_BEGIN_ARG_INFO_EX(arginfo_phpskirt__construct, 0, 0, 2)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_ARRAY_INFO(0, extensions, 0)
ZEND_END_ARG_INFO()

#define PHPSKIRT_HAS_EXTENSION(name)  (table != NULL && zend_hash_exists(table, name,strlen(name)+1) == 1)

static void php_phpskirt__get_flags(HashTable *table, unsigned int *enabled_extensions_p, unsigned int *render_flags_p)
{
	TSRMLS_FETCH();
	unsigned int render_flags = HTML_EXPAND_TABS;
	unsigned int extensions = 0;

	/* filter_html */
	if (PHPSKIRT_HAS_EXTENSION("filter_html")) {
		render_flags |= HTML_SKIP_HTML;
	}

	/* no_image */
	if (PHPSKIRT_HAS_EXTENSION("no_image")) {
		render_flags |= HTML_SKIP_IMAGES;
	}

	/* no_links */
	if (PHPSKIRT_HAS_EXTENSION("no_links")) {
		render_flags |= HTML_SKIP_LINKS;
	}

	/* filter_style */
	if (PHPSKIRT_HAS_EXTENSION("filter_styles")) {
		render_flags |= HTML_SKIP_STYLE;
	}

	/* safelink */
	if (PHPSKIRT_HAS_EXTENSION("safelink")) {
		render_flags |= HTML_SAFELINK;
	}

	if (PHPSKIRT_HAS_EXTENSION("generate_toc")) {
		render_flags |= HTML_TOC;
	}

	if (PHPSKIRT_HAS_EXTENSION("hard_wrap")) {
		render_flags |= HTML_HARD_WRAP;
	}

	if (PHPSKIRT_HAS_EXTENSION("gh_blockcode")) {
		render_flags |= HTML_GITHUB_BLOCKCODE;
	}

	if (PHPSKIRT_HAS_EXTENSION("xhtml")) {
		render_flags |= HTML_USE_XHTML;
	}

	/**
	 * Markdown extensions -- all disabled by default 
	 */
	if (PHPSKIRT_HAS_EXTENSION("autolink")) {
		extensions |= MKDEXT_AUTOLINK;
	}

	if (PHPSKIRT_HAS_EXTENSION("no_intraemphasis")) {
		extensions |= MKDEXT_NO_INTRA_EMPHASIS;
	}

	if (PHPSKIRT_HAS_EXTENSION("tables")) {
		extensions |= MKDEXT_TABLES;
	}

	if (PHPSKIRT_HAS_EXTENSION("fenced_code")) {
		extensions |= MKDEXT_FENCED_CODE;
	}

	if (PHPSKIRT_HAS_EXTENSION("strikethrough")) {
		extensions |= MKDEXT_STRIKETHROUGH;
	}

	if (PHPSKIRT_HAS_EXTENSION("lax_htmlblock")) {
		extensions |= MKDEXT_LAX_HTML_BLOCKS;
	}

	if (PHPSKIRT_HAS_EXTENSION("space_header")) {
		extensions |= MKDEXT_SPACE_HEADERS;
	}

	*enabled_extensions_p = extensions;
	*render_flags_p = render_flags;
}

static void phpskirt__render(PHPSkirtRendererType render_type, INTERNAL_FUNCTION_PARAMETERS)
{
	struct buf input_buf, *output_buf;
	struct mkd_renderer phpskirt_render;
	unsigned int enabled_extensions = 0, render_flags = 0;
	char *buffer;
	int buffer_len = 0;
	HashTable *table;

	buffer = Z_STRVAL_P(zend_read_property(phpskirt_class_entry, getThis(),"data",sizeof("data")-1, 0 TSRMLS_CC));
	buffer_len = strlen(buffer);
	
	memset(&input_buf, 0x0, sizeof(struct buf));
	input_buf.data = buffer;
	input_buf.size = strlen(buffer);
	
	output_buf = bufnew(128);
	bufgrow(output_buf, strlen(buffer) * 1.2f);

	if(Z_TYPE_P(zend_read_property(phpskirt_class_entry, getThis(),"extensions",sizeof("extensions")-1, 0 TSRMLS_CC)) != IS_NULL) {
		table = Z_ARRVAL_P(zend_read_property(phpskirt_class_entry, getThis(),"extensions",sizeof("extensions")-1, 0 TSRMLS_CC));
	}
	php_phpskirt__get_flags(table, &enabled_extensions, &render_flags);

	switch (render_type) {
		case PHPSKIRT_RENDER_HTML:
			upshtml_renderer(&phpskirt_render, render_flags);
			break;
		case PHPSKIRT_RENDER_TOC:
			upshtml_toc_renderer(&phpskirt_render);
			break;
		default:
			RETURN_FALSE;
	}

	ups_markdown(output_buf, &input_buf, &phpskirt_render, enabled_extensions);

	if (Z_BVAL_P(zend_read_property(phpskirt_class_entry, getThis(),"enable_pants",sizeof("enable_pants")-1, 0 TSRMLS_CC))) {
		struct buf *smart_buf = bufnew(128);
		upshtml_smartypants(smart_buf, output_buf);
		RETVAL_STRINGL(smart_buf->data, smart_buf->size,1);
		bufrelease(smart_buf);
	} else {
		RETVAL_STRINGL(output_buf->data, output_buf->size,1);
	}

	bufrelease(output_buf);
	upshtml_free_renderer(&phpskirt_render);
}

/* {{{ proto string __construct(string $string [, array $extensions])
	setup Upskirt extension */
PHP_METHOD(phpskirt, __construct)
{
	zval *extensions = NULL;
	char *buffer;
	int buffer_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"s|a",&buffer, &buffer_len, &extensions) == FAILURE) {
		return;
	}

	add_property_string_ex(getThis(),"data",sizeof("data"),buffer,1 TSRMLS_CC);

	if (extensions == NULL) {
		MAKE_STD_ZVAL(extensions);
		array_init(extensions);
	}
    add_property_zval_ex(getThis(),"extensions",sizeof("extensions"),extensions TSRMLS_CC);
}
/* }}} */

/* {{{ proto void __destruct()
	cleanup variables */
PHP_METHOD(phpskirt, __destruct)
{
	zval *extensions;
	
	extensions = zend_read_property(phpskirt_class_entry, getThis(),"extensions",sizeof("extensions")-1, 0 TSRMLS_CC);
	if(extensions != NULL && Z_TYPE_P(extensions) != IS_NULL) {
		zval_ptr_dtor(&extensions);
	}
}
/* }}} */

/* {{{ proto string to_html()
	Returns converted HTML string */
PHP_METHOD(phpskirt, to_html)
{
	phpskirt__render(PHPSKIRT_RENDER_HTML,INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ proto string __toString()
	Returns converted HTML string */
PHP_METHOD(phpskirt, __toString)
{
	phpskirt__render(PHPSKIRT_RENDER_HTML,INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ proto string to_toc()
	Returns table of contents*/
PHP_METHOD(phpskirt, to_toc)
{
	phpskirt__render(PHPSKIRT_RENDER_TOC,INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static function_entry php_phpskirt_methods[] = {
	PHP_ME(phpskirt, __construct, arginfo_phpskirt__construct, ZEND_ACC_PUBLIC)
	PHP_ME(phpskirt, __destruct,  NULL, ZEND_ACC_PUBLIC)
	PHP_ME(phpskirt, to_html, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(phpskirt, to_toc,  NULL, ZEND_ACC_PUBLIC)
	PHP_ME(phpskirt, __toString,  NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


PHP_MINIT_FUNCTION(phpskirt) {
	php_phpskirt_init(TSRMLS_C);
	return SUCCESS;
}


PHP_MINFO_FUNCTION(phpskirt)
{
	return SUCCESS;
}

zend_module_entry phpskirt_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"phpskirt",
	NULL,				    /* Functions */
	PHP_MINIT(phpskirt),	/* MINIT */
	NULL,					/* MSHUTDOWN */
	NULL,					/* RINIT */
	NULL,					/* RSHUTDOWN */
	PHP_MINFO(phpskirt),	/* MINFO */
#if ZEND_MODULE_API_NO >= 20010901
	PHP_PHPSKIRT_EXTVER,
#endif
	STANDARD_MODULE_PROPERTIES
};

void php_phpskirt_init(TSRMLS_D)
{
	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "Upskirt", php_phpskirt_methods);
	phpskirt_class_entry = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_null(phpskirt_class_entry, "extensions", sizeof("extensions")-1,  ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(phpskirt_class_entry, "enable_pants", sizeof("enable_pants")-1,  ZEND_ACC_PUBLIC TSRMLS_CC);
}


#ifdef COMPILE_DL_PHPSKIRT
ZEND_GET_MODULE(phpskirt)
#endif
