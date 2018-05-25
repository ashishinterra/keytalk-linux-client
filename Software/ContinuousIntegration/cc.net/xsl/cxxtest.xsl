<?xml version="1.0"?>
<xsl:stylesheet  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:output method="html"/>
  
  <xsl:template match="/">
    <xsl:variable name="tests.list" select="/cruisecontrol/build"/>
    <xsl:variable name="tests.count" select="count($tests.list)"/>
    <xsl:variable name="testsuites.list" select="/cruisecontrol/build/testsuite"/>
    <xsl:variable name="testsuites.count" select="count($testsuites.list)"/>
    <xsl:variable name="testcases.list" select="/cruisecontrol/build/testsuite/testcase"/>
    <xsl:variable name="testcases.count" select="count($testcases.list)"/>
    <xsl:variable name="failedtestcases.list" select="/cruisecontrol/build/testsuite/testcase/failure"/>
    <xsl:variable name="failedtestcases.count" select="count($failedtestcases.list)"/>
    <xsl:variable name="succeededtestcases.count" select="$testcases.count - $failedtestcases.count"/>
    <xsl:variable name="warntestcases.list" select="/cruisecontrol/build/testsuite/testcase/warning"/>
    <xsl:variable name="warntestcases.count" select="count($warntestcases.list)"/>
    <xsl:variable name="exceptions.list" select="/cruisecontrol/exception"/>
    <xsl:variable name="exceptions.count" select="count($exceptions.list)"/>
    
    <table class="section-table" cellpadding="2" cellspacing="0" border="0" width="98%">
        <!-- Tests -->
        <tr>
          <td class="sectionheader" colspan="5">
            Tests executed (<xsl:value-of select="$tests.count"/>).
            Test suites executed (<xsl:value-of select="$testsuites.count"/>).
            Test cases executed (<xsl:value-of select="$testcases.count"/>).
            Succeeded (<xsl:value-of select="$succeededtestcases.count"/>).
            Errors (<xsl:value-of select="$failedtestcases.count"/>).
            Warnings (<xsl:value-of select="$warntestcases.count"/>).
            Exceptions (<xsl:value-of select="$exceptions.count"/>).
          </td>
        </tr>

        <!-- When build fails because of exception (e.g. timeout) most variables above will not be set, so we simply show the test output as is-->
        <xsl:for-each select="/cruisecontrol/build/buildresults/message">
           <tr>
             <td class="section-data" valign="top">
                <xsl:value-of select="text()"/>
              </td>
            </tr>   
        </xsl:for-each>
    </table>
  </xsl:template>

</xsl:stylesheet>
