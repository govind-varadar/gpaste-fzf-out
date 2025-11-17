#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdint.h>
#include <stdbool.h>

bool read0 = false;

#define _FPRINTF_ENDL(stream, format, ...) \
	do { \
		fprintf(stream, format, ##__VA_ARGS__); \
		fputc( read0 ? '\0' : '\n', stream); \
	} while (0)

void usage(const char *progname)
{
	fprintf(stdout, "Usage: %s [options]\n", progname);
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -h, --help       Show this help message\n");
}

char *get_gpaste_history_file()
{
	char *filename = getenv("GPASTE_HISTORY_FILE");
	char *history = getenv("GPASTE_HISTORY");
	char *home = getenv("HOME");
	ssize_t len;

	if (filename)
		return filename;

	if (!home) {
		fprintf(stderr, "Error: HOME environment variable not set.\n");
		fprintf(stderr, "Please set HOME or GPASTE_HISTORY_FILE environment variable.\n");
		return NULL;
	}

	if (history) {
		if (!home) {
			fprintf(stderr, "Error: HOME environment variable not set.\n");
			fprintf(stderr, "Please set HOME or GPASTE_HISTORY_FILE environment variable.\n");
			return NULL;
		}
		len = strlen(home) + strlen("/.local/share/gpaste/") + strlen(history) + strlen(".xml") + 1;
		filename = malloc(len);
		if (!filename) {
			fprintf(stderr, "Error: Memory allocation failed.\n");
			return NULL;
		}
		snprintf(filename, len, "%s/.local/share/gpaste/%s.xml", home, history);
		/* Freed on program exit */
		return filename;
	} else {
		pid_t pid;
		int pipefd[2];

		if (pipe(pipefd) == -1) {
			fprintf(stderr, "Error: Pipe creation failed.\n");
			return NULL;
		}

		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "Error: Fork failed.\n");
			return NULL;
		}

		if (pid == 0) {
			/* Child process */
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[0]);
			close(pipefd[1]);
			execlp("gpaste-client", "gpaste-client", "get-history", NULL);
			exit(0);
		} else {
			ssize_t n;
			char buffer[4096];

			/* Parent process */
			close(pipefd[1]);
			waitpid(pid, NULL, 0);
			n = read(pipefd[0], buffer, sizeof(buffer) - 1);
			if (!n || n == -1 || n == sizeof(buffer) -1) {
				fprintf(stderr, "Error: Failed to read from gpaste-client: %ld.\n", n);
				close(pipefd[0]);
				return NULL;
			}
			/* gpaste-client appends a newline, remove it */
			buffer[n - 1] = '\0';
			close(pipefd[0]);
			len = strlen(home) + strlen("/.local/share/gpaste/") + strlen(buffer) + strlen(".xml") + 1;
			filename = malloc(len);
			if (!filename) {
				fprintf(stderr, "Error: Memory allocation failed.\n");
				return NULL;
			}
			snprintf(filename, len, "%s/.local/share/gpaste/%s.xml", home, buffer);
			/* Buffer freed on program exit */
			return filename;
		}
	}

	return NULL;
}

enum kind {
	KIND_NONE = 0,
	KIND_TEXT,
	KIND_IMAGE,
	KIND_FILE,
	KIND_URIS,
};

char *get_value_of_kind_node(xmlNode *node)
{
	xmlNode *cur_node = NULL;
	xmlAttr *attr = node->properties;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		xmlAttr *attr = cur_node->properties;

		if (cur_node->type != XML_ELEMENT_NODE)
			continue;
		if (strcmp((char *)cur_node->name, "value"))
			continue;
		if (attr)
			continue;
		return (char *)xmlNodeListGetString(node->doc, cur_node->children, 1);
	}

	return NULL;
}

void parse_xml_node(xmlNode *node)
{
	xmlNode *cur_node = NULL;

	for (cur_node = node; cur_node; cur_node = cur_node->next) {
		xmlAttr *attr = cur_node->properties;
		int kind = KIND_NONE;
		char *uuid = NULL;
		char *kind_str = NULL;
		char *value = NULL;

		if (cur_node->type != XML_ELEMENT_NODE)
			continue;

		while (attr) {
			if (!strcmp((const char *)attr->name, "kind")) {
				xmlChar *kind_value = xmlNodeListGetString(cur_node->doc, attr->children, 1);

				if (!strcmp((const char *)kind_value, "Text"))
					kind = KIND_TEXT;
				else if (!strcmp((const char *)kind_value, "Image"))
					kind = KIND_IMAGE;
				else if (!strcmp((const char *)kind_value, "file"))
					kind = KIND_FILE;
				else if (!strcmp((const char *)kind_value, "Uris"))
					kind = KIND_URIS;

				kind_str = (char *)kind_value;
			} else if (!strcmp((const char *)attr->name, "uuid")) {
				uuid = (char *)xmlNodeListGetString(cur_node->doc, attr->children, 1);
			}
			attr = attr->next;
		}
		if (kind && uuid && kind_str) {
			value = get_value_of_kind_node(cur_node);
			_FPRINTF_ENDL(stdout, "kind:%s:uuid:%s:%s\n", kind_str, uuid, value);
		}
		if (uuid)
			xmlFree(uuid);
		if (kind_str)
			xmlFree(kind_str);
		if (value)
			xmlFree(value);
		parse_xml_node(cur_node->children);
	}
}

int print_xml_history(const char *filename)
{
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *cur_node = NULL;

	LIBXML_TEST_VERSION
	doc = xmlReadFile(filename, NULL, 0);
	if (doc == NULL) {
		fprintf(stderr, "Error: Could not parse file %s\n", filename);
		return -ENOENT;
	}

	root_element = xmlDocGetRootElement(doc);
	if (!root_element) {
		fprintf(stderr, "Error: Empty XML document %s\n", filename);
		xmlFreeDoc(doc);
		return -EINVAL;
	}
	printf("\n");
	parse_xml_node(root_element);

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return 0;
}

int main(int argc, char *argv[])
{
	char *filename = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage(argv[0]);
			return 0;
		} else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--file")) {
			i++;
			if (i < argc) {
				filename = argv[i];
			} else {
				fprintf(stderr, "Error: Missing filename after %s\n", argv[i - 1]);
				return 1;
			}
		} else if (!strcmp(argv[i], "--read0")) {
			read0 = true;
		} else {
			fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
			usage(argv[0]);
			return 1;
		}
	}

	if (!filename) {
		filename = get_gpaste_history_file();
		if (!filename) {
			fprintf(stderr, "Error: Could not determine history file.\n");
			return -ENOENT;
		}
	}
	_FPRINTF_ENDL(stdout, ":::Using file: %s", filename);

	return print_xml_history(filename);
}

