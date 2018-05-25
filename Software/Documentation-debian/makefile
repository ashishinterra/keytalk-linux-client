include ../mk/keytalk.common.mk

ifdef RESEPT_LINUX_CLIENT

GENERATED_DOCUMENTS=KeyTalk_LinuxClient_for_Apache.pdf KeyTalk_LinuxClient_for_Apache_remote_deployment.pdf

release debug: $(GENERATED_DOCUMENTS)

clean:
	rm -f $(GENERATED_DOCUMENTS)

%.pdf: %.txt
	@echo Generating $@...
	@pandoc --css markdown7.css --from markdown --to html --standalone $< > $<.html
	@xvfb-run --server-args="-screen 0, 1024x768x24" wkhtmltopdf $<.html $@ > /dev/null
	@rm -f ./$<.html

endif

ifdef RESEPT_SERVER

VERSION := $(shell cut -d'-' -f 2 ../Server/version)
DOCS_PACKAGE=../Server/Projects/Export/keytalk.svr.doc-$(VERSION).tar

release debug:
	@echo Generating server documentation...
	@tar -cf $(DOCS_PACKAGE) KeyTalkServerAdminManualNotes.docx KeyTalkProtocols.pdf
	@tar -uf $(DOCS_PACKAGE) -C ../Server/Projects/config/ create-shared-db-tables.sql create-mod-mysql-db-tables.sql

clean:
	@rm -f $(DOCS_PACKAGE)

endif

install:
