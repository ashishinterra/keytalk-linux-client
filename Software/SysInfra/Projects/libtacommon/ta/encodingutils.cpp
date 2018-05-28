#include "encodingutils.h"
#include "pngutils.h"
#include "utils.h"
#include "common.h"

#include "openssl/bn.h"
#include "openssl/x509.h"
#include "boost/regex.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/algorithm/string/replace.hpp"

using boost::property_tree::ptree;
using std::string;
using std::wstring;
using std::vector;

namespace ta
{
    namespace EncodingUtils
    {

        //
        // Private API
        //
        namespace
        {
            const char* AsciiSpecialHtmlEntities[][2] = {
                {"&", "&amp;"},{"\"", "&quot;"},{"<", "&lt;"},{">", "&gt;"}
            };
            static const char SafeUrlEncodingChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-/";

            std::string hexEncode(const unsigned char ch)
            {
                const unsigned char hex[]= { (unsigned char)(ch/16),
                                             (unsigned char)(ch%16)
                                           };
                string myRetVal = "%";

                if (hex[0] <= 9)
                    myRetVal+= (hex[0] + '0');
                else
                    myRetVal+= (hex[0] - 10 + 'A');

                if (hex[1] <= 9)
                    myRetVal+= (hex[1] + '0');
                else
                    myRetVal+= (hex[1] - 10 + 'A');

                return myRetVal;
            }

            // "{\"\":[\"one\",\"two\"]}\n" -> "[\"one\", \"two\"]\n"
            string extractJsonArray(const string& aJson)
            {
                string myRetVal = aJson;
                string::size_type myPos = myRetVal.find_first_of('[');
                if (myPos == string::npos)
                {
                    TA_THROW_MSG(std::invalid_argument, "Invalid JSON dictionary with array: " + aJson);
                }
                myRetVal = myRetVal.substr(myPos);

                myPos = myRetVal.find_last_of(']');
                if (myPos == string::npos)
                {
                    TA_THROW_MSG(std::invalid_argument, "Invalid JSON dictionary with array: " + aJson);
                }
                myRetVal = myRetVal.substr(0, myPos+1);

                return myRetVal + "\n";
            }
        }


        //
        // Public API
        //

        string toBase64(const vector<unsigned char>& anSrc, bool aSingleLine)
        {
            if (anSrc.empty())
                return "";
            BIO* myMemBio	  = BIO_new(BIO_s_mem());
            BIO* myBase64Bio  = BIO_new(BIO_f_base64());
            if (aSingleLine)
                BIO_set_flags(myBase64Bio, BIO_FLAGS_BASE64_NO_NL);
            BIO* myBio        = BIO_push(myBase64Bio, myMemBio);
            int myWritten;
            try { myWritten= BIO_write(myBio, ta::getSafeBuf(anSrc), boost::numeric_cast<int>(anSrc.size())); }
            catch (boost::bad_numeric_cast& e) {	TA_THROW_MSG(EncodeError, e.what());	}
            if (myWritten < 0 || static_cast<size_t>(myWritten) < anSrc.size())
            {
                BIO_free_all(myBio);
                TA_THROW_MSG(EncodeError, "BIO_write failed");
            }
            (void)BIO_flush(myBio);
            BUF_MEM* myEncBuf;
            if (BIO_get_mem_ptr(myBio, &myEncBuf) < 0 || myEncBuf->length <= 0)
            {
                BIO_free_all(myBio);
                TA_THROW_MSG(EncodeError, "BIO_get_mem_ptr failed");
            }
            size_t myLen = (myEncBuf->data[myEncBuf->length-1] == '\n') ?
                           static_cast<size_t>(myEncBuf->length-1) :
                           static_cast<size_t>(myEncBuf->length);
            string myRetVal(myEncBuf->data, myLen);
            BIO_free_all(myBio);
            return myRetVal;
        }

        vector<unsigned char> fromBase64(const string& anSrc, bool aSingleLine)
        {
            vector<char> mySrcVec(anSrc.begin(), anSrc.end());
            if (mySrcVec.empty() || mySrcVec.back() != '\n')
                mySrcVec.push_back('\n');
            BIO* myBase64Bio  = BIO_new(BIO_f_base64());
            if (aSingleLine)
                BIO_set_flags(myBase64Bio, BIO_FLAGS_BASE64_NO_NL);
            BIO* myMemBio = NULL;
            try { myMemBio = BIO_new_mem_buf(ta::getSafeBuf(mySrcVec), boost::numeric_cast<int>(mySrcVec.size())); }
            catch (boost::bad_numeric_cast& e) {	TA_THROW_MSG(DecodeError, e.what());	}
            myMemBio = BIO_push(myBase64Bio, myMemBio);
            vector<unsigned char> myRetVal;
            while (true)
            {
                unsigned char myBuf[128] = {};
                int myRead = BIO_read(myMemBio, myBuf, sizeof(myBuf));
                if (myRead < 0)
                {
                    BIO_free_all(myMemBio);
                    TA_THROW_MSG(DecodeError, "BIO_read failed");
                }
                myRetVal.insert(myRetVal.end(), myBuf, myBuf + myRead);
                if (static_cast<unsigned int>(myRead) < sizeof(myBuf))
                    break;
            }
            BIO_free_all(myMemBio);
            return myRetVal;
        }

        string urlEncode(const string& anSrc)
        {
            string myRetVal;
            const string mySafeChars = SafeUrlEncodingChars;

            string::size_type mySize = anSrc.size();
            for (string::size_type pos = 0; pos < mySize; ++pos)
            {
                unsigned char ch = anSrc[pos];
                if (mySafeChars.find(ch) != string::npos)
                {
                    myRetVal+= ch;
                    continue;
                }
                if (ch == ' ')
                {
                    myRetVal+= "+";
                }
                else
                {
                    myRetVal+= hexEncode(ch);
                }
            }
            return myRetVal;
        }

        std::string urlDecode(const std::string& anSrc)
        {
            std::string myRetVal;
            string::size_type mySize = anSrc.size();
            for (string::size_type pos = 0; pos < mySize; ++pos)
            {
                unsigned char ch = anSrc[pos];
                if (ch == '+')
                {
                    myRetVal += ' ';
                    continue;
                }
                if ((ch == '%') && (pos + 2 < mySize))
                {
                    unsigned char hex[2];
                    for (string::size_type i = pos + 1; i < pos + 3; ++i)
                    {
                        if ((anSrc[i] >= '0') && (anSrc[i] <= '9'))
                            hex[i - pos - 1]= anSrc[i] - '0';
                        else if ((anSrc[i] >= 'A') && (anSrc[i] <= 'F'))
                            hex[i - pos - 1]= 10 + anSrc[i] - 'A';
                        else if ((anSrc[i] >= 'a') && (anSrc[i] <= 'f'))
                            hex[i - pos - 1]= 10 + anSrc[i] - 'a';
                        else
                            hex[i - pos - 1]= 16;
                    }
                    if (hex[0] < 16 && hex[1] < 16)
                    {
                        ch = (unsigned char) 16*hex[0] + hex[1];
                        pos+= 2;
                    }
                }
                myRetVal+= ch;
            }
            return myRetVal;
        }

        string dataUriEncodePngImage(const vector<unsigned char>& aPngBlob)
        {
            // Validate, will raise exception if not PNG
            ta::PngUtils::getPngInfo(aPngBlob);

            const string myDataUri = str(boost::format("data:image/png;base64,%s") % ta::EncodingUtils::toBase64(aPngBlob, true));
            return myDataUri;
        }

        vector<unsigned char> dataUriDecodePngImage(const string& anEncodedPng)
        {
            boost::regex myRegEx("data:image/png;base64,\\s*([a-zA-Z0-9\\-\\+\\/\\=]+)");
            boost::cmatch myMatch;
            if (!regex_match(anEncodedPng.c_str(), myMatch, myRegEx))
            {
                TA_THROW_MSG(std::invalid_argument,
                             boost::format("Cannot parse data-URI encoded PNG image from: '%s'") % anEncodedPng);
            }

            const string myEncodedPngBlob = myMatch[1];
            const vector<unsigned char> myPngBlob = ta::EncodingUtils::fromBase64(myEncodedPngBlob, true);
            try
            {
                ta::PngUtils::getPngInfo(myPngBlob);
            }
            catch (...)
            {
                TA_THROW_MSG(std::invalid_argument,
                             boost::format("Invalid PNG image found in data-URI encoded PNG BLOB: '%s'") % myEncodedPngBlob);
            }

            return myPngBlob;
        }


        string htmlEncode(const string& anSrc)
        {
            string myEncoded = anSrc;
            for (size_t i = 0; i < sizeof(AsciiSpecialHtmlEntities)/sizeof(AsciiSpecialHtmlEntities[0]); ++i)
                boost::replace_all(myEncoded, AsciiSpecialHtmlEntities[i][0], AsciiSpecialHtmlEntities[i][1]);
            return myEncoded;
        }

        wstring toWide(const string& aStr)
        {
            size_t myWsLen = mbstowcs(NULL, aStr.c_str(), 0);
            if (myWsLen == (size_t)(-1))
                TA_THROW_MSG(EncodeError, "Invalid multibyte string");
            wchar_t* myPtr = new wchar_t[myWsLen];
            mbstowcs(myPtr, aStr.c_str(), myWsLen);
            wstring myRetVal(myPtr, myWsLen);
            delete []myPtr;
            return myRetVal;
        }
        string toMbyte(const wstring& aWstr)
        {
            size_t myMbyteLen = wcstombs(NULL, aWstr.c_str(), aWstr.length()*sizeof(wchar_t)+1);
            if (myMbyteLen == (size_t)(-1))
                TA_THROW_MSG(EncodeError, "Invalid wide character string");
            char* myPtr = new char[myMbyteLen];
            wcstombs(myPtr, aWstr.c_str(), myMbyteLen);
            string myRetVal(myPtr, myMbyteLen);
            delete []myPtr;
            return myRetVal;
        }

        ptree toTree(const ta::StringArray& anArray)
        {
            ptree tree;
            foreach(const string& elem, anArray)
            {
                ptree treeElem;
                treeElem.put_value(elem);
                tree.push_back(std::make_pair("", treeElem));
            }
            return tree;
        }
        ptree toTree(const ta::StringDict& aDict)
        {
            ptree tree;
            foreach(const ta::StringDict::value_type& kv, aDict)
            {
                tree.put(kv.first, kv.second);
            }
            return tree;
        }
        ptree toTree(const ta::StringDictArray& aStringDictArray)
        {
            ptree myTree;
            foreach (const ta::StringDict& itemDict, aStringDictArray)
            {
                ptree myTreeItem;
                foreach (const ta::StringDict::value_type& kv, itemDict)
                {
                    myTreeItem.put(kv.first, kv.second);
                }
                myTree.push_back(std::make_pair("", myTreeItem));
            }
            return myTree;
        }

        vector<string> toStringArray(const ptree& aTree)
        {
            vector<string> myRetVal;
            for (ptree::const_iterator it = aTree.begin(), end = aTree.end(); it != end; ++it)
            {
                myRetVal.push_back(string(it->second.data()));
            }
            return myRetVal;
        }
        ta::StringDict toStringDict(const ptree& aTree)
        {
            ta::StringDict myRetVal;
            for(ptree::const_iterator it = aTree.begin(), end = aTree.end(); it != end; ++it)
            {
                myRetVal[string(it->first.data())] = string(it->second.data());
            }
            return myRetVal;
        }
        ta::StringDictArray toStringDictArray(const boost::property_tree::ptree& aTree)
        {
            ta::StringDictArray myRetVal;
            for (ptree::const_iterator it = aTree.begin(), end = aTree.end(); it != end; ++it)
            {
                const ptree myTreeItem = it->second;
                myRetVal.push_back(toStringDict(myTreeItem));
            }
            return myRetVal;
        }

        string toJson(const ta::StringArray& anArray)
        {
            //@note if we directly use boost::property_tree to JSON-serialize array, it will produce something like "{\"\":\"one\",\"\":\"two\" }" iso more conventional "[\"one\", \"two\"]"
            // Though both forms above are valid JSON, however the first form might not be understood by some JSON parsers (aka MySQL) and is simply less readable
            // The trick below makes sure we yield the second (more conventional) form

            if (anArray.empty())
            {
                return "[]\n";
            }
            ptree tree;
            tree.push_back(std::make_pair("", toTree(anArray)));
            const string myJson = toJson(tree); // will produce "{\"\":[\"one\",\"two\"]}\n"
            return extractJsonArray(myJson);// will produce "[\"one\", \"two\"]\n"
        }
        string toJson(const ta::StringDict& aStringDict)
        {
            return toJson(toTree(aStringDict));
        }
        string toJson(const ta::StringDictArray& aStringDictArray)
        {
            //@not see toJson(const ta::StringArray&) for the explanation of the trick below

            if (aStringDictArray.empty())
            {
                return "[]\n";
            }
            ptree tree;
            tree.push_back(std::make_pair("", toTree(aStringDictArray)));

            const string myJson = toJson(tree);
            return extractJsonArray(myJson);
        }
        string toJson(const ptree& aTree)
        {
            std::ostringstream stream;
            write_json(stream, aTree, false);
            return stream.str();
        }

        vector<string> jsonToStringArray(const string& aJson)
        {
            return toStringArray(jsonToTree(aJson));
        }
        ta::StringDict jsonToStringDict(const string& aJson)
        {
            return toStringDict(jsonToTree(aJson));
        }
        ta::StringDictArray jsonToStringDictArray(const string& aJson)
        {
            return toStringDictArray(jsonToTree(aJson));
        }
        boost::property_tree::ptree jsonToTree(const std::string& aJson)
        {
            std::stringstream myJsonSs;
            myJsonSs << aJson;

            ptree pt;
            boost::property_tree::read_json(myJsonSs, pt);
            return pt;
        }
    }
}
