#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lib/mlrutil.h"
#include "containers/slls.h"
#include "lib/string_builder.h"
#include "input/peek_file_reader.h"

#define TERMIND_RS  0x1111
#define TERMIND_FS  0x2222
#define TERMIND_EOF 0x3333

typedef struct _field_wrapper_t {
	char* contents;
	int   termind;
} field_wrapper_t;

typedef struct _record_wrapper_t {
	slls_t* contents;
	int   at_eof;
} record_wrapper_t;

static field_wrapper_t get_csv_field_not_dquoted(peek_file_reader_t* pfr, string_builder_t* psb) {
	// xxx need pfr_advance_past_or_die ...
	// xxx "\"," etc. will be encoded in the rfc_csv_reader_t ctor -- this is just sketch
	printf("\n");
	printf("ENTER\n");
	while (TRUE) {
		if (pfr_at_eof(pfr)) {
			printf("--case 1\n");
			printf("EXIT\n");
			return (field_wrapper_t) { .contents = sb_is_empty(psb) ? NULL: sb_finish(psb), .termind = TERMIND_EOF };
		} else if (pfr_next_is(pfr, ",\xff", 2)) {
			printf("--case 2\n");
			if (!pfr_advance_past(pfr, ",\xff")) {
				fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
				exit(1);
			}
			printf("EXIT\n");
			return (field_wrapper_t) { .contents = sb_finish(psb), .termind = TERMIND_EOF };
		} else if (pfr_next_is(pfr, ",", 1)) {
			printf("--case 3\n");
			if (!pfr_advance_past(pfr, ",")) {
				fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
				exit(1);
			}
			printf("EXIT\n");
			return (field_wrapper_t) { .contents = sb_finish(psb), .termind = TERMIND_FS };
		} else if (pfr_next_is(pfr, "\r\n", 2)) {
			printf("--case 4\n");
			if (!pfr_advance_past(pfr, "\r\n")) {
				fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
				exit(1);
			}
			printf("EXIT\n");
			return (field_wrapper_t) { .contents = sb_finish(psb), .termind = TERMIND_RS };
		} else {
			//pfr_dump(pfr);
			char c = pfr_read_char(pfr);
			printf("--case 5 %c [%02x]\n", isprint(c) ? c : '?', c);
			//pfr_dump(pfr);
			sb_append_char(psb, c);
			//sb_append_char(psb, pfr_read_char(pfr));
		}
	}
}

static field_wrapper_t get_csv_field_dquoted(peek_file_reader_t* pfr, string_builder_t* psb) {
	// xxx need pfr_advance_past_or_die ...
	if (!pfr_advance_past(pfr, "\"")) {
		fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
		exit(1);
	}
	while (TRUE) {
		if (pfr_at_eof(pfr)) {
			// xxx imbalanced-dquote error
			fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
			exit(1);
		} else if (pfr_next_is(pfr, "\"\xff", 2)) {
			if (!pfr_advance_past(pfr, "\"\xff")) {
				fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
				exit(1);
			}
			return (field_wrapper_t) { .contents = sb_finish(psb), .termind = TERMIND_EOF };
		} else if (pfr_next_is(pfr, "\",", 2)) {
			if (!pfr_advance_past(pfr, "\",")) {
				fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
				exit(1);
			}
			return (field_wrapper_t) { .contents = sb_finish(psb), .termind = TERMIND_FS };
		} else if (pfr_next_is(pfr, "\"\r\n", 3)) {
			if (!pfr_advance_past(pfr, "\"\r\n")) {
				fprintf(stderr, "xxx k0d3 me up b04k3n b04k3n b04ken %d\n", __LINE__);
				exit(1);
			}
			return (field_wrapper_t) { .contents = sb_finish(psb), .termind = TERMIND_RS };
		} else {
			sb_append_char(psb, pfr_read_char(pfr));
		}
	}
}

field_wrapper_t get_csv_field(peek_file_reader_t* pfr, string_builder_t* psb) {
	field_wrapper_t wrapper;
	if (pfr_at_eof(pfr)) {
		wrapper.contents = NULL;
		wrapper.termind = TERMIND_EOF;
		return wrapper;
	} else if (pfr_next_is(pfr, "\"", 1)) {
		return get_csv_field_dquoted(pfr, psb);
	} else {
		return get_csv_field_not_dquoted(pfr, psb);
	}
}

record_wrapper_t get_csv_record(peek_file_reader_t* pfr, string_builder_t* psb) {
	slls_t* fields = slls_alloc();
	record_wrapper_t rwrapper;
	rwrapper.contents = fields;
	rwrapper.at_eof = FALSE;
	while (TRUE) {
		field_wrapper_t fwrapper = get_csv_field(pfr, psb);
		if (fwrapper.termind == TERMIND_EOF) {
			rwrapper.at_eof = TRUE;
		}
		if (fwrapper.contents != NULL) {
			printf("CONT=>>%s<<[%d]\n", fwrapper.contents, (int)strlen(fwrapper.contents));
			slls_add_with_free(fields, fwrapper.contents);
		}
		if (fwrapper.termind != TERMIND_FS)
			break;
	}
	printf("FLEN=%d\n", fields->length);
	printf("FEOF=%d\n", rwrapper.at_eof);
	if (fields->length == 0 && rwrapper.at_eof) {
		slls_free(fields);
		rwrapper.contents = NULL;
	}
	return rwrapper;
}

int main() {
	FILE* fp = stdin;
	peek_file_reader_t* pfr = pfr_alloc(fp, 32);
	string_builder_t sb;
	string_builder_t* psb = &sb;
	sb_init(psb, 1024);

	while (TRUE) {
		record_wrapper_t rwrapper = get_csv_record(pfr, psb);
		if (rwrapper.contents != NULL) {
			printf("++++ [NF=%d]\n", rwrapper.contents->length);
			for (sllse_t* pe = rwrapper.contents->phead; pe != NULL; pe = pe->pnext) {
				printf("  [%s]\n", pe->value);
			}
			slls_free(rwrapper.contents);
		}
		if (rwrapper.at_eof)
			break;
	}

	return 0;
}
