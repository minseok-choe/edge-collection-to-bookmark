#include "curl/curl.h"
#include "sqlite/sqlite3.h"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <cstring>
#include <fstream>

#include <Processenv.h>

#define EXPCOL_VERSION "1.0.2"

//#define DEBUG
#ifdef DEBUG
int debugcount = 0;
#endif

using typename std::string, std::unordered_map, std::pair, std::vector, std::map, std::wstring;
using std::make_pair;

enum class sortcriteria {
    def,
    name,
    add,
    last
};

sortcriteria folder_sortcriteria = sortcriteria::def;
bool folder_reverse = false;
sortcriteria bookmark_sortcriteria = sortcriteria::def;
bool bookmark_reverse = false;

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

    string faviconurl_to_base64(const char * url)
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
                return "";
            }

            string ico = base64::encode(fp->data, fp->pointer);
            delete fp;
            return ico;
        }
        else
        {
          return "";
        }

    }

} // namespace favicon

const char * PAGE_TITLE = "Bookmarks";
const char * PAGE_H1 = "Bookmarks";
const char * TOP_FOLDER_NAME = "From Collections";
constexpr time_t maxtime() {
    time_t x = 0;
    time_t y = 1;

    while(x!=y) {
        x = x << 1;
        x += 1;

        y = y << 1;
        y += 1;
    }

    return x;
}
time_t file_add = maxtime();
time_t file_last = 0;

bool download_favicon = false;
bool download_boost = false;
int favicon_numbers = 0;
unordered_map<string, pair<string, string>> url_to_base64string; //extenstion, base64

typedef std::string bookmark_id;
typedef std::string collection_id;
typedef time_t position;

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
    string urlshortening(string url) {
        string s = after(url, ":");
        size_t index;
        for(index = 0; index < s.length(); ++index) {if(s[index]!='/') break;}
        s = s.substr(index, s.length() -index);
        s = before(s, "/");
        return s;
    }    
    time_t str_to_time_t(string str)
    {
        if (str.find(".") != str.size()) //millisecond랑 time_t섞여있음 소수점 있으면 밀리초로 가정
        {
            str = parse::before(str, ".");
            if (str.length()>3) str = str.substr(0, str.length() -3);
            else str = "0";
        }

        time_t date = 0;
        for(const char& digit : str)
        {
            date *= 10;
            date += digit - '0';
        }

        return date;
    }
    inline time_t str_to_time_t(const char * str) {return str_to_time_t(string(str));}
    int str_to_int(string str)
    {
        int date = 0;
        for(const char& digit : str)
        {
            date *= 10;
            date += digit - '0';
        }

        return date;
    }
};

class bookmark {
    private:
    public:
    string id;
    int reducedid;
    string href;
    time_t add_date;
    time_t last_modified;
    string iconurl;
    string name;

    int print(std::ofstream& os)
    {

        os
        << "            <DT><A HREF=\"" << this->href
        << "\" ADD_DATE=\"" << this->add_date
        << "\"";

        string thisurl = this->iconurl;
        if (download_boost) thisurl = parse::urlshortening(thisurl);

        if (url_to_base64string.find(thisurl) != url_to_base64string.end() && url_to_base64string[thisurl].first != "fail")
        {
            os
            << " ICON=\"data:image/"
            << url_to_base64string[thisurl].first
            << ";base64,"
            << url_to_base64string[thisurl].second
            << "\"";
        }

        os
        << ">"
        << this->name
        #ifdef DEBUG
        << "   -" 
        << ++debugcount
        << "th"
        #endif
        << "</A></DT>"
        << '\n';
        return 0;
    }
};

class folder {
    public:
    string id;
    int reducedid;
    string name;
    time_t add_date;
    time_t last_modified;
    map<pair<string, int>, bookmark *> bookmarks_sortbyname;
    map<pair<time_t, int>, bookmark *> bookmarks_sortbytime;
 
    int print(std::ofstream& os)
    {
        os
        << "        <DT><H3 ADD_DATE=\""
        << this->add_date
        << "\" LAST_MODIFIED=\""
        << this->last_modified
        << "\">"
        //<< "Col "
        //<< " : "
        << this->name
        #ifdef DEBUG
        << "   Count:"
        << bookmarks_sortbyname.size()
        << ' '
        << bookmarks_sortbytime.size()
        #endif
        << "</H3>\n";

        os << "        <DL><p>\n";

        if (this->bookmarks_sortbyname.empty())
        for (auto& [key, value]: this->bookmarks_sortbytime)
        {value->print(os);}
        else
        for (auto& [key, value]: this->bookmarks_sortbyname)
        {value->print(os);}

        os << "        </DL></DT><p>\n"; ///디렉토리 닫는 DL

        return 0;
    }
};

map<pair<string, int>, folder *> collection_sortbyname;
map<pair<time_t, int>, folder *> collection_sortbytime;

unordered_map<bookmark_id, collection_id> bookmarkid_to_collectionid;
unordered_map<collection_id, folder *> collection_tagging;
unordered_map<bookmark_id, bookmark *> bookmark_tagging;


vector<string> url_favicon_candidates;

void printProgress(const char * message, double percentage) {
    /*
    This function code is from
    https://stackoverflow.com/questions/14539867/how-to-display-a-progress-indicator-in-pure-c-c-cout-printf
    */
    const int PBWIDTH = 60;

    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%24s%3d%% [%.*s%*s]", message, val, lpad, "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||", rpad, "");
    fflush(stdout);
}

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
    << file_add
    << "\" LAST_MODIFIED=\""
    << file_last
    << "\" PERSONAL_TOOLBAR_FOLDER=\"true\">"
    << TOP_FOLDER_NAME
    << "</H3></DT>\n"
    << "    <DL><p>\n"; //컬렉션감사는dl
    
    if (collection_sortbyname.empty())
    for (auto& [key, value]: collection_sortbytime)
    {value->print(os);}
    else
    for (auto& [key, value]: collection_sortbyname)
    {value->print(os);}

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

namespace callback
{
    int count_rows (void * index, int arg, char * col[], char * colname[])
    {   
        ++favicon_numbers;
        return 0;
    }
    int download_icon (void * index, int argc, char * argv[], char * colname[])
        {   
            string url(argv[0]);
            string extension = parse::rafter(parse::before(url, "?"), ".");

            if (download_boost){
                string shortenurl = parse::urlshortening(url);
                if (url_to_base64string.find(shortenurl) == url_to_base64string.end())
                {
                    string ico = favicon::faviconurl_to_base64(url.c_str());
                    if (ico!="") url_to_base64string[shortenurl] = make_pair(extension, ico);
                    else url_to_base64string[shortenurl] = make_pair("fail", "");
                }
            }
            else if (url_to_base64string.find(url) == url_to_base64string.end())
            {
                string ico = favicon::faviconurl_to_base64(url.c_str());
                if (ico!="") url_to_base64string[url] = make_pair(extension, ico);
                else url_to_base64string[url] = make_pair("fail", "");
            }

            double progress = ((double) *((int*)index)) / (double) favicon_numbers;
            if (progress >= 0.99) printProgress("Downloading Favicon", 1.0);
            else printProgress("Downloading Favicon", progress);

            *((int*)index) += 1;

            return 0;
        }


    int make_collection_vector (void * index, int argc, char * argv[], char * colname[]) {

            folder * reading = new folder();

            reading->id = argv[0];
            reading->reducedid = *( (int*)index );
            reading->add_date = parse::str_to_time_t(argv[1]);
            reading->last_modified = parse::str_to_time_t(argv[2]);
            reading->name = argv[3];

            file_add = std::min(file_add, reading->add_date);
            file_last = std::min(file_last, reading->last_modified);

            if (collection_tagging.find(reading->id)!=collection_tagging.end()) std::cout << "중복되는 id 존재! : col\n";
            collection_tagging[reading->id] = reading;

            switch (folder_sortcriteria)
            {
            case sortcriteria::name:
                collection_sortbyname[make_pair(reading->name, reading->reducedid)] = reading;
                break;
            
            default:
                collection_sortbytime[make_pair((folder_sortcriteria == sortcriteria::last ? reading->last_modified : reading->add_date), reading->reducedid)] = reading;
                break;
            }
           
            *((int*)index) += 1;

            return 0;

        }

        int mark_to_folder_initialize (void * index, int argc, char * argv[], char * colname[])
        {   
            bookmark * reading = new bookmark();
            bookmark_tagging[string(argv[0])] = reading;

            reading->id = argv[0];
            reading->reducedid = parse::str_to_int(argv[2]);
            
            bookmarkid_to_collectionid[string(argv[0])] = argv[1];
            return 0;
        }

    int read_items (void * index, int argc, char * argv[], char * colname[])
        {   
            if (strcmp(argv[11],"website")!=0) {return 0;}
            else if(argv[4]==nullptr) {return 0;}
            
            string jsoned(argv[4]);
            jsoned = parse::after(jsoned, "\"url\"");
            jsoned = parse::after(jsoned, ":");
            jsoned = parse::after(jsoned, "\"");
            jsoned = parse::before(jsoned, "\"");

            bookmark * reading = bookmark_tagging[string(argv[0])];

            folder * parent = collection_tagging[bookmarkid_to_collectionid[reading->id]];

            char empty[1] = {'\0'};

            reading->add_date = parse::str_to_time_t(argv[1]);
            reading->last_modified = parse::str_to_time_t(argv[2]);
            reading->href = jsoned;
            reading->name = (argv[3]==nullptr ? "Untitled" : argv[3]);
            reading->iconurl = (argv[6]==nullptr ? empty : argv[6]);

            switch (folder_sortcriteria)
            {
            case sortcriteria::name:
                parent->bookmarks_sortbyname[make_pair(reading->name, reading->reducedid)] = reading;
                break;
            
            default:
                parent->bookmarks_sortbytime[make_pair((bookmark_sortcriteria == sortcriteria::last ? reading->last_modified : reading->add_date), reading->reducedid)] = reading;
                break;
            }

            file_add = std::min(file_add, reading->add_date);
            file_last = std::min(file_last, reading->last_modified);
            
            return 0;
        }
} // namespace callback


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
    const char * b_filter;
    const char * f_filter;

    unordered_map<string, char> argmap;
    {

    argmap["-f"] = 'f';
    argmap["--favicon"] = 'f';

    
    argmap["-ff"] = '2';
    argmap["--fastfavicon"] = '2';

    argmap["-i"] = 'i';
    argmap["--input"] = 'i';
    argmap["-o"] = 'o';
    argmap["--output"] = 'o';
    argmap["-p"] = 'p';
    argmap["--profile"] = 'p';
    
    argmap["--help"] = 'h';
    argmap["--version"] = 'v';

    argmap["-sf"] = 'c';
    argmap["-sb"] = 'b';
    }
    
    unordered_map<string, sortcriteria> sortoption;
    {
    sortoption["add_date"] = sortcriteria::add;
    sortoption["last_modified"] = sortcriteria::last;
    sortoption["name"] = sortcriteria::name;
    }

    for(int index = 1; index < argc; ++index)
    {
        
        constexpr int printalin = 40;

        switch (argmap[argv[index]])
        {
            case 'h':
            {


            std::cout << "\n"
            << "Path\n";

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


            
            std::cout << "\n"
            << "Favicon\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-f | --favicon]"
            << color::reset
            << "Download favicon from bookmark url, may take times longer\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "[-ff | --fastfavicon]"
            << color::reset
            << "Download favicon faster by assuming wesites cannot have different icons per-page. This implies -f on.\n";



            std::cout << "\n"
            << "Sort Filters\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "-sf [name|add_date|last_modified]"
            << color::reset
            << "Set sort filter of folders\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "-sb [name|add_date|last_modified]"
            << color::reset
            << "Set sort filter of bookmarks\n";


            std::cout << "\n"
            << "About Informations\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "--help"
            << color::reset
            << "Show this text\n";

            std::cout << color::yellow;
            std::cout.width(printalin);
            std::cout << std::left
            << "--version"
            << color::reset
            << "Show version of program\n";

            //<< "-s --sort [default/name/day_created/recently_used]\n"
            ;

            return 0;
            }
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
        
        case 'b': //bookmark-sort
            if (index+1 < argc && sortoption.find(argv[index+1])!=sortoption.end())
            {
                bookmark_sortcriteria = sortoption.at(argv[index+1]);
                b_filter = argv[index+1];
                ++index;
            }
            else
            {
                std::cout << "Error occured while parsing arguments: "
                << color::red << "no or invalid argument for bookmark sort filter\n" << color::reset;
                return 0;
            }
            break;
        
        case 'c': //folder / collection-sort
            if (index+1 < argc && sortoption.find(argv[index+1])!=sortoption.end())
            {
                folder_sortcriteria = sortoption.at(argv[index+1]);
                f_filter = argv[index+1];
                ++index;
            }
            else
            {
                std::cout << "Error occured while parsing arguments: "
                << color::red << "no or invalid argument for folder(collection) sort filter\n" << color::reset;
                return 0;
            }
            break;


        case '2':
            download_favicon = true;
        case 'f':
            download_favicon = true;
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

        if (bookmark_sortcriteria != sortcriteria::def)
        std::cout << "Bookmark sorted by : " << color::brightyellow << b_filter << color::reset << '\n';

        if (folder_sortcriteria != sortcriteria::def)
        std::cout << "Folder sorted by : " << color::brightyellow << f_filter << color::reset << '\n';

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

    case 3:
        std::cout << "Error occured at opening database file. Is this path valid?: "
        << expand_path(INPUT_PATH) << '\n';
        break;

    case 1:
        std::cout << "Error occured at reading database file. Is this path valid?: "
        << expand_path(INPUT_PATH) << '\n';
        break;

    case 2:
        std::cout << "Error occured making output file. Is this path valid or accessible with current permission?: "
        << OUTPUT_PATH
        << "\n";
        break;

    default:
        break;
    }
}
