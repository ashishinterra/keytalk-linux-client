Default DEMO certificate tree
================================

This cert tree is used:
----------------------------
- in development environment
- as a default tree delivered with KeyTalk server firmware

Tree generation instructions
-----------------------------
1. Generate the certificate tree with KeyTalk server
2. Create .der from .pem counterparts when applicable
3. Copy DevID-related CAs to DevID
4. Regenerate Backend authd certificate and key using Server/Projects/CA/certs.sh
5. Replace verification CA and regenerate SSL certificate for r4webdemo.gotdns.com (use Server/Projects/CA/certs.sh). Upload and apply these changes on r4webdemo.gotdns.com
6. Recreate RCCDs in Client/TestProjects/Common/RCCDs and upload them to r4webdemo.gotdns.com/rccds
7. Recreate certificates under Client/TestProjects/testReseptInstaller/linux/apache/localhost-ssl-cert/ using KeyTalk Linux client
