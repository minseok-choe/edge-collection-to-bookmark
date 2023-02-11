#include"sqlite/sqlite3.h"
#include "curl/curl.h"

#include <optional>
#include<iostream>
#include<vector>
#include<string>
#include<map>
#include<unordered_map>
#include<cstring>
#include<fstream>

#include<Processenv.h>

#define EXPCOL_VERSION "1.0.0"

using typename std::string, std::unordered_map, std::pair, std::vector, std::map, std::wstring, std::optional;
using std::make_pair;

namespace favicon
{
    namespace base64
    {
      const std::string encodetable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      
      std::string encode(const char *text, size_t textsize)
      {
          unsigned char input[3] = { 0,0,0 };
          unsigned char output[4] = { 0,0,0,0 };
          size_t   index, i;
          const char *p = text, *plen = text + textsize - 1;
          
          std::string s = "";
          s.reserve((4 * (textsize / 3)) + (textsize % 3 ? 4 : 0) + 1);

          for (i = 0, p = text; p <= plen; i++, p++) {
              index = i % 3;
              input[index] = *p;
              if (index == 2 || p == plen) {
                  output[0] = ((input[0] & 0xFC) >> 2);
                  output[1] = ((input[0] & 0x3) << 4) | ((input[1] & 0xF0) >> 4);
                  output[2] = ((input[1] & 0xF) << 2) | ((input[2] & 0xC0) >> 6);
                  output[3] = (input[2] & 0x3F);

                  s.push_back(encodetable[output[0]]);
                  s.push_back(encodetable[output[1]]);
                  s.push_back(index == 0 ? '=' : encodetable[output[2]]);
                  s.push_back(index <  2 ? '=' : encodetable[output[3]]);

                  input[0] = input[1] = input[2] = 0;
              }
          }
          return s;
      }
    }

    class virtual_file {
      public:
      const size_t vfsize = 50000000ULL; //50MiB (52.4MB)
      char * data;
      size_t pointer = 0;

      virtual_file() {data = new char[vfsize];}
      ~virtual_file() {delete [] data;}
    };

    size_t write_data(void *ptr, size_t size, size_t nmemb, virtual_file *stream) {
        size_t written;
        for(written = 0; written < size * nmemb; ++written) stream->data[stream->pointer++] = ((char*)ptr)[written];
        return written;
    }

    std::optional<std::string> faviconurl_to_base64(const char * url)
    {
      
        CURL *curl;
        virtual_file * fp;
        CURLcode res;

        curl_version_info_data * vinfo = curl_version_info(CURLVERSION_NOW);

        /*
        if(vinfo->features & CURL_VERSION_SSL){
            printf("CURL: SSL enabled\n");
        }else{
            printf("CURL: SSL not enabled\n");
        }
        */

        curl = curl_easy_init();
        if (curl) {
            fp = new virtual_file;
            /* Setup the https:// verification options. Note we   */
            /* do this on all requests as there may be a redirect */
            /* from http to https and we still want to verify     */
            curl_easy_setopt(curl, CURLOPT_URL, url);
            //curl_easy_setopt(curl, CURLOPT_CAINFO, "./ca-bundle.crt");
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK)
            {
                delete fp;
                return std::nullopt;
            }

            optional ico = base64::encode(fp->data, fp->pointer);
            delete fp;
            return ico;
        }
        else
        {
          return std::nullopt;
        }

    }

} // namespace favicon


const char * PAGE_TITLE = "Bookmarks";
const char * PAGE_H1 = "Bookmarks";
const char * TOP_FOLDER_NAME = "From Collections";

bool download_favicon = false;
bool download_boost = false;
int favicon_numbers = 0;

typedef std::string bookmark_id;
typedef std::string collection_id;
typedef long long int position;
typedef time_t collection_sort_criteria;
typedef position bookmark_sort_criteria;

const char * INPUT_PATH = R"(%localappdata%\Microsoft\Edge\User Data\Default\Collections\collectionsSQLite)";
const char * OUTPUT_PATH = R"(%UserProfile%\exported from edge collections.html)";

string p_path;
string p_path_top = R"(%localappdata%\Microsoft\Edge\User Data\)";
string p_name;
string p_path_bot = R"(\Collections\collectionsSQLite)";

const size_t MAXSIZE = 32768;
char pathdata[MAXSIZE];

namespace color {
    const char * black      = "\033[0;30m";
    //const char * red        = "\033[0;31m";
    const char * red        = "\033[0;91m";
    const char * green      = "\033[0;32m";
    const char * yellow     = "\033[0;33m";
    const char * brightyellow     = "\033[0;93m";
    //const char * blue       = "\033[0;34m";
    const char * blue       = "\033[0;94m";
    const char * purple     = "\033[0;35m";
    const char * cyan       = "\033[0;36m";
    const char * white      = "\033[0;37m";
    const char * reset      = "\033[0m";
};

namespace parse {
    string after(string s, string token) { return s.substr(s.find(token) + token.length(), s.length() - s.find(token) - token.length() );}
    string before(string s, string token){ return s.substr(0, s.find(token));}
    string rafter(string s, string token) { return s.substr(s.rfind(token) + token.length(), s.length() - s.rfind(token) - token.length() );}
    string rbefore(string s, string token){ return s.substr(0, s.rfind(token));}

    string shortening(string url)
    {
        string s = after(url, ":");
        size_t index;
        for(index = 0; index < s.length(); ++index) {if(s[index]!='/') break;}
        s = s.substr(index, s.length() -index);
        s = before(s, "/");
        return s;
    }
};
class bookmark {
    private:
    public:
    string href;
    time_t add_date;
    string iconurl;
    string name;

    bookmark (string href_para, time_t add_date_para, const char * icon_para, string name_para)
     : href(href_para), add_date(add_date_para), name(name_para)
     {
        if (icon_para != nullptr)
        {
            iconurl = string(icon_para);
        }
        else
        {
            iconurl = "";
        }
     };
};
class folder {
    public:
    string name;
    time_t add_date;
    time_t last_modified;
    map<position, bookmark *> bookmarks;
};

map<collection_sort_criteria, collection_id> collectionlist;
unordered_map<bookmark_id, pair<collection_id, position>> bookmarkid_to_collectionid;
unordered_map<collection_id, folder *> collection_tagging;

vector<string> url_favicon_candidates;
unordered_map<string, pair<string, string>> url_to_base64string;

void printProgress(const char * message, double percentage) {
    /*
    This function code is from
    https://stackoverflow.com/questions/14539867/how-to-display-a-progress-indicator-in-pure-c-c-cout-printf
    */
    const int PBWIDTH = 60;

    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%20s%3d%% [%.*s%*s]", message, val, lpad, "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||", rpad, "");
    fflush(stdout);
}

namespace callback
        {
            
        int make_collection_vector(void *iter, int argc, char * argv[], char * azColName[]) {

            folder * reading = new folder();

            time_t date = 0;
            for(size_t index = 0; argv[1][index] != '\0' && argv[1][index] != '.'; ++index)
            {
                date *= 10;
                date += argv[1][index] - '0';
            }
            reading->add_date = date;

            date = 0;
            for(size_t index = 0; argv[2][index] != '\0' && argv[2][index] != '.'; ++index)
            {
                date *= 10;
                date += argv[2][index] - '0';
            }
            reading->last_modified = date;
            reading->name = argv[3];

            collection_tagging.insert( make_pair(argv[0], reading) );
            collectionlist.insert( make_pair((collection_sort_criteria) *((int*)iter) , argv[0]) );
            
            *((int*)iter) += 1;

            return 0;
        }

        int mark_to_folder_initialize(void *iter, int argc, char * argv[], char * azColName[])
        {   
            position pos = 0;
            for(size_t index = 0; argv[2][index] != '\0'; ++index)
            {
                pos *= 10;
                pos += argv[2][index] - '0';
            }
            bookmarkid_to_collectionid.insert(make_pair(argv[0], make_pair(argv[1], pos)));
            return 0;
        }

        int read_items(void *iter, int argc, char * argv[], char * azColName[])
        {   
            if (strcmp(argv[11],"website")!=0) {return 0;}

            string jsoned(argv[4]);
            jsoned = parse::after(jsoned, "\"url\"");
            jsoned = parse::after(jsoned, ":");
            jsoned = parse::after(jsoned, "\"");
            jsoned = parse::before(jsoned, "\"");
            
            time_t date = 0;
            for(size_t index = 0; argv[1][index] != '\0' && argv[1][index] != '.'; ++index)
            {
                date *= 10;
                date += argv[1][index] - '0';
            }

            bookmark_id id = argv[0];

            bookmark * reading = new bookmark(jsoned, date, argv[6], argv[3]);
            collection_tagging[bookmarkid_to_collectionid[id].first]->bookmarks.insert(make_pair(bookmarkid_to_collectionid[id].second, reading));
            
            *((int*)iter) += 1;

            return 0;
        }

        int count_rows(void *iter, int argc, char * argv[], char * azColName[])
        {   
            *((int*)iter) += 1;
            return 0;
        }

        int download_icon(void *iter, int argc, char * argv[], char * azColName[])
        {   
            string url(argv[0]);
            string extension = parse::rafter(parse::before(url, "?"), ".");

            if (download_boost){
                string shortenurl = parse::shortening(url);
                if (url_to_base64string.find(shortenurl) == url_to_base64string.end())
                {
                    optional ico = favicon::faviconurl_to_base64(url.c_str());
                    if (ico) url_to_base64string[shortenurl] = make_pair(extension, ico.value());
                    else url_to_base64string[shortenurl] = make_pair("fail", "");
                }
            }
            else if (url_to_base64string.find(url) == url_to_base64string.end())
            {
                optional ico = favicon::faviconurl_to_base64(url.c_str());
                if (ico) url_to_base64string[url] = make_pair(extension, ico.value());
                else url_to_base64string[url] = make_pair("fail", "");
            }

            double progress = ((double) *((int*)iter)) / ((double) favicon_numbers);
            if (progress >= 0.99) printProgress("Downloading Favicon", 1.0);
            else printProgress("Downloading Favicon", progress);

            *((int*)iter) += 1;

            return 0;
        }
} // namespace callback

int printHTML(std::ofstream& os)
{
    os
    << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<!-- This is an automatically generated file.\n    It will be read and overwritten.\n    Do Not Edit! -->\n<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n"
    
    << "<TITLE>"
    << PAGE_TITLE
    << "</TITLE>\n"

    << "<H1>"
    << PAGE_H1
    << "</H1>\n"

    << "<DL><p>\n";

    os
    << "    <DT><H3 ADD_DATE=\""
    << 0
    << "\" LAST_MODIFIED=\"0\" PERSONAL_TOOLBAR_FOLDER=\"true\">"
    << TOP_FOLDER_NAME
    << "</H3></DT>\n"
    << "    <DL><p>\n"; //컬렉션감사는dl
    
    
    for(const auto& [key, value] : collectionlist)
    {
        os
        << "        <DT><H3 ADD_DATE=\""
        << collection_tagging[value]->add_date
        << "\" LAST_MODIFIED=\""
        << collection_tagging[value]->last_modified
        << "\">"
        //<< "Col "
        //<< " : "
        << collection_tagging[value]->name
        << "</H3>\n";

        os << "        <DL><p>\n";

        for(const auto& [underkey, b] : collection_tagging[value]->bookmarks)
        {
            os
            << "            "
            << "<DT><A HREF=\"" << b->href
            << "\" ADD_DATE=\"" << b->add_date
            << "\"";

            string thisurl = b->iconurl;
            if (download_boost) thisurl = parse::shortening(thisurl);

            if (url_to_base64string.find(thisurl) != url_to_base64string.end() && url_to_base64string[thisurl].first != "fail")
            {
                os
                << " ICON=\""
                << "data:image/"
                << url_to_base64string[thisurl].first
                << ";base64,"
                << url_to_base64string[thisurl].second
                << "\"";
            }

            os
            << ">"
            << b->name << "</A></DT>"
            << '\n';
        }

        os << "        </DL></DT><p>\n"; ///디렉토리 닫는 DL
    }

    os
    << "    </DL><p>\n" ///컬렉션 닫는 DL
    << "</DL><p>\n"; ///전체를감싸는DL

    return 0;
}

const char * expand_path(const char * path)
{
    ExpandEnvironmentStrings(path, pathdata, MAXSIZE-1);
    return pathdata;
}

int convert(const char * filepath, const char * outputpath)
{
    
    sqlite3 *db;

    int rc;

    rc = sqlite3_open(expand_path(filepath), &db);
    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", 
        sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;

        return 1;
    }
    

    int index;
    char * err_msg;

    if (download_favicon)
    {
        rc = sqlite3_exec(db, "SELECT * FROM favicons", callback::count_rows, &favicon_numbers, &err_msg);
        if(rc != SQLITE_OK){
            //fprintf(stderr, "Some error occur: %s\n", 
            //sqlite3_errmsg(db));
            sqlite3_close(db);
            db = nullptr;

            return 1;
        }

        std::cout << color::brightyellow;    
        rc = sqlite3_exec(db, "SELECT * FROM favicons", callback::download_icon, (index = 0, &index), &err_msg);
        std::cout << color::reset << '\n';
        if(rc != SQLITE_OK){
        //fprintf(stderr, "Some error occur: %s\n", 
        //sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;

        return 1;
    }
    }

    rc = sqlite3_exec(db, "SELECT * FROM collections_items_relationship", callback::mark_to_folder_initialize, (index = 0, &index), &err_msg);
    if(rc != SQLITE_OK){
        //fprintf(stderr, "Some error occur: %s\n", 
        //sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;

        return 1;
    }

    rc = sqlite3_exec(db, "SELECT * FROM collections", callback::make_collection_vector, (index = 0, &index), &err_msg);

    if(rc != SQLITE_OK){
        //fprintf(stderr, "Some error occur: %s\n", 
        //sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;

        return 1;
    }

    rc = sqlite3_exec(db, "SELECT * FROM items", callback::read_items, (index = 0, &index), &err_msg);

    if(rc != SQLITE_OK){
        //fprintf(stderr, "Some error occur: %s\n", 
        //sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;

        return 1;
    }

    sqlite3_close(db);

    std::ofstream os;
    os.open(expand_path(outputpath), std::ios_base::binary);
    if(os.fail())
    {
        os.close();
        return 2;
    }

    printHTML(os);
    
    os.close();

    return 0;
}

int main(int argc, char* argv[])
{
    
    unordered_map<string, char> argmap;
    argmap["-h"] = 'h';
    argmap["--help"] = 'h';
    argmap["-f"] = 'f';
    argmap["--favicon"] = 'f';
    argmap["-i"] = 'i';
    argmap["--input"] = 'i';
    argmap["-o"] = 'o';
    argmap["--output"] = 'o';
    argmap["-p"] = 'p';
    argmap["--profile"] = 'p';
    argmap["-b"] = 'b';
    argmap["--boost"] = 'b';
    
    argmap["-v"] = 'v';
    argmap["--version"] = 'v';
    

    for(int index = 1; index < argc; ++index)
    {
        
        constexpr int printalin = 40;

        switch (argmap[argv[index]])
        {
            case 'h':
            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-h | --help]"
            << color::reset
            << "Show this text\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-i | --input] filepath"
            << color::reset
            << "Set edge collection db path manually\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-o | --output] filepath"
            << color::reset
            << "Set location and name of exported HTML file\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-p | --profile] profile_folder_name"
            << color::reset
            << "Set input path using profile name. -p \"default\" is same as default behaviour.\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-f | --favicon]"
            << color::reset
            << "Download favicon from bookmark url, may take times longer\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-b | --boost]"
            << color::reset
            << "Download favicon faster by assuming wesites cannot have different icons per-page. This implies -f on.\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-v | --version]"
            << color::reset
            << "Show version of program\n";

            //<< "-s --sort [default/name/day_created/recently_used]\n"
            ;

            return 0;
            break;

        case 'p':
            if (index+1 < argc && argmap.find(argv[index+1])==argmap.end())
            {
                p_name = argv[index+1];
                p_path = p_path_top + p_name + p_path_bot;
                INPUT_PATH = p_path.c_str();

                ++index;
            }
            else
            {
                std::cout << "Error occured while parsing arguments: "
                << color::red << "profile name invalid or missing?\n" << color::reset;
                return 0;
            }
            break;

        case 'f':
            download_favicon = true;
            break;

        case 'b':
            download_favicon = true;
            download_boost = true;
        break;

        case 'i':
            if (index+1 < argc && argmap.find(argv[index+1])==argmap.end())
            {
                INPUT_PATH = argv[index+1];
                ++index;
            }
            else
            {
                std::cout << "Error occured while parsing arguments: "
                << color::red << "input path invalid or missing?\n" << color::reset;
                return 0;
            }
            break;
        
        case 'o':
            if (index+1 < argc && argmap.find(argv[index+1])==argmap.end())
            {
                OUTPUT_PATH = argv[index+1];
                ++index;
            }
            else
            {
                std::cout << "Error occured while parsing arguments: "
                << color::red << "output path invalid or missing?\n" << color::reset;
                return 0;
            }
        break;
        
        case 'v':
            std::cout << "Version " << EXPCOL_VERSION << '\n';
            return 0;
            break;

        default:
            {
                std::cout << "Error occured while parsing arguments: "
                << color::red << "Unknown argument \"" << argv[index] << "\"\n" << color::reset;
                std::cout << "Use " << color::yellow << "--help" << color::reset << " to see available options\n";
                return 0;
            }
        break;
        }
    }

    if (download_favicon) std::cout << color::yellow << "Favicon download might take time\n" << color::reset;

    int res = convert(INPUT_PATH, OUTPUT_PATH);

    switch (res)
    {
    case /* constant-expression  */0:
        std::cout
        << "Export completed\nfrom "
        << color::brightyellow
        << "\""
        << expand_path(INPUT_PATH)
        << "\""
        << color::reset
        << "\nto   "
        << color::blue
        << "\""
        << expand_path(OUTPUT_PATH)
        << "\""
        << color::reset
        << "\n";
        break;

    case 1:
        std::cout << "Error occured at database file.\n";
        break;

    case 2:
        std::cout << "Error occured making output file. Is this path valid or accessible with current permission?: "
        << OUTPUT_PATH
        << "\n";

    default:
        break;
    }
}
