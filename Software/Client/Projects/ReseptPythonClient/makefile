CLIENT_VERSION:=$(shell cut -d '=' -f 2 ../../version)
PACKAGE=../Export/KeyTalkClient-$(CLIENT_VERSION).python.demo.tgz
DEPENDENCIES=common_config.py pcacert.pem commcacert.pem KeyTalkProtocols.pdf

.PHONY : release debug clean install

all: release

release debug: $(PACKAGE)

$(PACKAGE): $(DEPENDENCIES) keytalk-client.py
	tar -czf $@ $?

common_config.py: ../../../Common/common_config.py
	cp $? ./

pcacert.pem commcacert.pem: ../../../CertKeys/CommunicationAndSigning/pcacert.pem ../../../CertKeys/CommunicationAndSigning/commcacert.pem
	cp $? ./

KeyTalkProtocols.pdf: ../../../Documentation/KeyTalkProtocols.pdf
	cp $? ./

clean:
	rm -f ${DEPENDENCIES} ${PACKAGE}

install:
