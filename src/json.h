#ifndef _JSON_H_
#define _JSON_H_

#include <istream>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/* Cataclysm-DDA homegrown JSON tools
 * copyright CC-BY-SA-3.0 2013 CleverRaven
 *
 * Consists of six JSON manipulation tools:
 * JsonIn - for low-level parsing of an input JSON stream
 * JsonOut - for outputting JSON
 * JsonObject - convenience-wrapper for reading JSON objects from a JsonIn
 * JsonArray - convenience-wrapper for reading JSON arrays from a JsonIn
 * JsonSerializer - inheritable interface for custom datatype serialization
 * JsonDeserializer - inheritable interface for custom datatype deserialization
 *
 * Further documentation can be found below.
 */
class JsonIn;
class JsonOut;
class JsonObject;
class JsonArray;
class JsonSerializer;
class JsonDeserializer;

/* JsonIn
 * ======
 *
 * The JsonIn class provides a wrapper around a std::istream,
 * with methods for reading JSON data directly from the stream.
 *
 * JsonObject and JsonArray provide higher-level wrappers,
 * and are a little easier to use in most cases,
 * but have the small overhead of indexing the members or elements before use.
 *
 * Typical JsonIn usage might be something like the following:
 *
 *     JsonIn jsin(myistream);
 *     // expecting an array of objects
 *     jsin.start_array(); // throws std::string if array not found
 *     while (!jsin.end_array()) { // end_array returns false if not the end
 *         JsonObject jo = jsin.get_object();
 *         ... // load object using JsonObject methods
 *     }
 *
 * The array could have been loaded into a JsonArray for convenience.
 * Not doing so saves one full pass of the data,
 * as the element positions don't have to be read beforehand.
 *
 * If the JSON structure is not as expected,
 * verbose error messages are provided, indicating the problem,
 * and the exact line number and byte offset within the istream.
 *
 *
 * Single-Pass Loading
 * -------------------
 *
 * A JsonIn can also be used for single-pass loading,
 * by passing off members as they arrive according to their names.
 *
 * Typical usage might be:
 *
 *     JsonIn jsin(&myistream);
 *     // expecting an array of objects, to be mapped by id
 *     std::map<std::string,my_data_type> myobjects;
 *     jsin.start_array();
 *     while (!jsin.end_array()) {
 *         my_data_type myobject;
 *         jsin.start_object();
 *         while (!jsin.end_object()) {
 *             std::string name = jsin.get_member_name();
 *             if (name == "id") {
 *                 myobject.id = jsin.get_string();
 *             } else if (name == "name") {
 *                 myobject.name = _(jsin.get_string().c_str());
 *             } else if (name == "description") {
 *                 myobject.description = _(jsin.get_string().c_str());
 *             } else if (name == "points") {
 *                 myobject.points = jsin.get_int();
 *             } else if (name == "flags") {
 *                 if (jsin.test_string()) { // load single string tag
 *                     myobject.tags.insert(jsin.get_string());
 *                 } else { // load tag array
 *                     jsin.start_array();
 *                     while (!jsin.end_array()) {
 *                         myobject.tags.insert(jsin.get_string());
 *                     }
 *                 }
 *             } else {
 *                 jsin.skip_value();
 *             }
 *         }
 *         myobjects[myobject.id] = myobject;
 *     }
 *
 * The get_* methods automatically verify and skip separators and whitespace.
 *
 * Using this method, unrecognized members are silently ignored,
 * allowing for things like "comment" members,
 * but the user must remember to provide an "else" clause,
 * explicitly skipping them.
 *
 * If an if;else if;... is missing the "else", it /will/ cause bugs,
 * so preindexing as a JsonObject is safer, as well as tidier.
 */
class JsonIn {
private:
    std::istream *stream;
    bool strict; // throw errors on non-RFC-4627-compliant input
    bool ate_separator;

    void skip_separator();
    void skip_pair_separator();
    void end_value();

public:
    JsonIn(std::istream &stream, bool strict = true);

    bool get_ate_separator() { return ate_separator; }
    void set_ate_separator(bool s) { ate_separator = s; }

    int tell(); // get current stream position
    void seek(int pos); // seek to specified stream position
    char peek(); // what's the next char gonna be?
    bool good(); // whether stream is ok

    // advance seek head to the next non-whitespace character
    void eat_whitespace();
    // or rewind to the previous one
    void uneat_whitespace();

    // quick skipping for when values don't have to be parsed
    void skip_member();
    void skip_string();
    void skip_value();
    void skip_object();
    void skip_array();
    void skip_true();
    void skip_false();
    void skip_null();
    void skip_number();

    // data parsing
    std::string get_string(); // get the next value as a string
    int get_int(); // get the next value as an int
    bool get_bool(); // get the next value as a bool
    double get_float(); // get the next value as a double
    std::string get_member_name(); // also strips the ':'
    JsonObject get_object();
    JsonArray get_array();

    // container control and iteration
    void start_array(); // verify array start
    bool end_array(); // returns false if it's not the end
    void start_object();
    bool end_object(); // returns false if it's not the end

    // type testing
    bool test_null();
    bool test_bool();
    bool test_number();
    bool test_int() { return test_number(); };
    bool test_float() { return test_number(); };
    bool test_string();
    bool test_array();
    bool test_object();

    // non-fatal reading into values by reference
    // returns true if the data was read successfully, false otherwise
    bool read(bool &b);
    bool read(int &i);
    bool read(unsigned int &u);
    bool read(float &f);
    bool read(double &d);
    bool read(std::string &s);
    bool read(JsonDeserializer &j);
    // array ~> vector
    template <typename T> bool read(std::vector<T> &v) {
        if (!test_array()) { return false; }
        try {
            start_array();
            v.clear();
            while (!end_array()) {
                T element;
                if (read(element)) { v.push_back(element); }
                else { skip_value(); }
            }
            return true;
        } catch (std::string e) { return false; }
    }
    // array ~> set
    template <typename T> bool read(std::set<T> &v) {
        if (!test_array()) { return false; }
        try {
            start_array();
            v.clear();
            while (!end_array()) {
                T element;
                if (read(element)) { v.insert(element); }
                else { skip_value(); }
            }
            return true;
        } catch (std::string e) { return false; }
    }
    // object ~> map
    template <typename T> bool read(std::map<std::string,T> &m) {
        if (!test_object()) { return false; }
        try {
            start_object();
            m.clear();
            while (!end_object()) {
                std::string name = get_member_name();
                T element;
                if (read(element)) { m[name] = element; }
                else { skip_value(); }
            }
            return true;
        } catch (std::string e) { return false; }
    }

    // error messages
    std::string line_number(int offset_modifier=0); // for occasional use only
    void error(std::string message, int offset=0); // ditto
    void rewind(int max_lines=-1, int max_chars=-1);
    std::string substr(size_t pos, size_t len=std::string::npos);
};


/* JsonOut
 * =======
 *
 * The JsonOut class provides a straightforward interface for outputting JSON.
 *
 * It wraps a std::ostream, providing methods for writing JSON data directly.
 *
 * Typical usage might be as follows:
 *
 *     JsonOut jsout(myostream);
 *     jsout.start_object();          // {
 *     jsout.member("type", "point"); // "type":"point"
 *     jsout.member("data");          // ,"data":
 *     jsout.start_array();           // [
 *     jsout.write(5)                 // 5
 *     jsout.write(9)                 // ,9
 *     jsout.end_array();             // ]
 *     jsout.end_object();            // }
 *
 * which writes {"type":"point","data":[5,9]} to myostream.
 *
 * Separators are handled automatically,
 * and the constructor also has an option for crude pretty-printing,
 * which inserts newlines and whitespace liberally, if turned on.
 *
 * Basic containers such as maps, sets and vectors,
 * as well as anything inheriting the JsonSerializer interface,
 * can be serialized automatically by write() and member().
 */
class JsonOut {
private:
    std::ostream *stream;
    bool pretty_print;
    bool need_separator;
    int indent_level;

public:
    JsonOut(std::ostream &stream, bool pretty_print=false);

    // punctuation
    void write_indent();
    void write_separator();
    void write_member_separator();
    void start_object();
    void end_object();
    void start_array();
    void end_array();

    // write data to the output stream as JSON
    void write_null();
    void write(const bool &b);
    void write(const int &i);
    void write(const unsigned &u);
    void write(const double &f);
    void write(const std::string &s);
    void write(const char *cstr) { write(std::string(cstr)); }
    void write(const JsonSerializer &thing);
    // vector ~> array
    template <typename T> void write(const std::vector<T> &v) {
        start_array();
        for (int i = 0; i < v.size(); ++i) {
            write(v[i]);
        }
        end_array();
    }
    // set ~> array
    template <typename T> void write(const std::set<T> &v) {
        start_array();
        typename std::set<T>::const_iterator it;
        for (it = v.begin(); it != v.end(); ++it) {
            write(*it);
        }
        end_array();
    }
    // map ~> object
    template <typename T> void write(const std::map<std::string,T> &m) {
        start_object();
        typename std::map<std::string,T>::const_iterator it;
        for (it = m.begin(); it != m.end(); ++it) {
            write(it->first);
            write_member_separator();
            write(it->second);
        }
        end_object();
    }

    // convenience methods for writing named object members
    void member(const std::string &name); // TODO: enforce value after
    void null_member(const std::string &name);
    template <typename T> void member(const std::string &name, const T &value)
    {
        member(name);
        write(value);
    }
};


/* JsonObject
 * ==========
 *
 * The JsonObject class provides easy access to incoming JSON object data.
 *
 * JsonObject maps member names to the byte offset of the paired value,
 * given an underlying JsonIn stream.
 *
 * It provides data by seeking the stream to the relevant position,
 * and calling the correct JsonIn method to read the value from the stream.
 *
 *
 * General Usage
 * -------------
 *
 *     JsonObject jo(jsin);
 *     std::string id = jo.get_string("id");
 *     std::string name = _(jo.get_string("name").c_str());
 *     std::string description = _(jo.get_string("description").c_str());
 *     int points = jo.get_int("points", 0);
 *     std::set<std::string> tags = jo.get_tags("flags");
 *     my_object_type myobject(id, name, description, points, tags);
 *
 * Here the "id", "name" and "description" members are required.
 * JsonObject will throw a std::string if they are not found,
 * identifying the problem and the current position in the input stream.
 *
 * Note that "name" and "description" are passed to gettext for translating.
 * Any members with string information that is displayed directly to the user
 * will need similar treatment, and may also need to be explicitly handled
 * in lang/extract_json_strings.py to be translatable.
 *
 * The "points" member here is not required.
 * If it is not found in the incoming JSON object,
 * it will be initialized with the default value given as the second parameter,
 * in this case, 0.
 *
 * get_tags() always returns an empty set if the member is not found,
 * and so does not require a default value to be specified.
 *
 *
 * Member Testing and Automatic Deserialization
 * --------------------------------------------
 *
 * JsonObjects can test for member type with has_int(name) etc.,
 * and for member existence with has_member(name).
 *
 * They can also read directly into compatible data structures,
 * including sets, vectors, maps, and any class inheriting JsonDeserializer,
 * using read(name, value).
 *
 * read() returns true on success, false on failure.
 *
 *     JsonObject jo(jsin);
 *     std::vector<std::string> messages;
 *     if (!jo.read("messages", messages)) {
 *         DebugLog() << "No messages.";
 *     }
 */
class JsonObject {
private:
    std::map<std::string, int> positions;
    int start;
    int end;
    bool final_separator;
    JsonIn *jsin;
    int verify_position(const std::string &name,
                        const bool throw_exception=true);

public:
    JsonObject(JsonIn &jsin);
    JsonObject(const JsonObject &jsobj);
    JsonObject() : positions(), start(0), end(0), jsin(NULL) {}
    ~JsonObject() { finish(); }

    void finish(); // moves the stream to the end of the object
    size_t size();
    bool empty();

    bool has_member(const std::string &name); // true iff named member exists
    std::set<std::string> get_member_names();
    std::string str(); // copy object json as string
    void throw_error(std::string err);
    void throw_error(std::string err, const std::string & name);
    // seek to a value and return a pointer to the JsonIn (member must exist)
    JsonIn* get_raw(const std::string &name);

    // values by name
    // variants with no fallback throw an error if the name is not found.
    // variants with a fallback return the fallback value in stead.
    bool get_bool(const std::string &name);
    bool get_bool(const std::string &name, const bool fallback);
    int get_int(const std::string &name);
    int get_int(const std::string &name, const int fallback);
    double get_float(const std::string &name);
    double get_float(const std::string &name, const double fallback);
    std::string get_string(const std::string &name);
    std::string get_string(const std::string &name, const std::string &fallback);

    // containers by name
    // get_array returns empty array if the member is not found
    JsonArray get_array(const std::string &name);
    std::vector<int> get_int_array(const std::string &name);
    std::vector<std::string> get_string_array(const std::string &name);
    // get_object returns empty object if not found
    JsonObject get_object(const std::string &name);
    // get_tags returns empty set if none found
    std::set<std::string> get_tags(const std::string &name);
    // TODO: some sort of get_map(), maybe

    // type checking
    bool has_null(const std::string &name);
    bool has_bool(const std::string &name);
    bool has_number(const std::string &name);
    bool has_int(const std::string &name) { return has_number(name); };
    bool has_float(const std::string &name) { return has_number(name); };
    bool has_string(const std::string &name);
    bool has_array(const std::string &name);
    bool has_object(const std::string &name);

    // non-fatally read values by reference
    // return true if the value was set, false otherwise.
    // return false if the member is not found.
    template <typename T> bool read(const std::string &name, T &t) {
        int pos = positions[name];
        if (pos <= start) { return false; }
        jsin->seek(pos);
        return jsin->read(t);
    }

    // useful debug info
    std::string line_number(); // for occasional use only
};


/* JsonArray
 * =========
 *
 * The JsonArray class provides easy access to incoming JSON array data.
 *
 * JsonArray stores only the byte offset of each element in the JsonIn stream,
 * and iterates or accesses according to these offsets.
 *
 * It provides data by seeking the stream to the relevant position,
 * and calling the correct JsonIn method to read the value from the stream.
 *
 * Arrays can be iterated over,
 * or accessed directly by element index.
 *
 * Elements can be returned as standard data types,
 * or automatically read into a compatible container.
 *
 *
 * Iterative Access
 * ----------------
 *
 *     JsonArray ja = jo.get_array("some_array_member");
 *     std::vector<int> myarray;
 *     while (ja.has_more()) {
 *         myarray.push_back(ja.next_int());
 *     }
 *
 * If the array member is not found, get_array() returns an empty array,
 * and has_more() is always false, so myarray will remain empty.
 *
 * next_int() and the other next_* methods advance the array iterator,
 * so after the final element has been read,
 * has_more() will return false and the loop will terminate.
 *
 * If the next element is not an integer,
 * JsonArray will throw a std::string indicating the problem,
 * and the position in the input stream.
 *
 * To handle arrays with elements of indeterminate type,
 * the test_* methods can be used, calling next_* according to the result.
 *
 * Note that this style of iterative access requires an element to be read,
 * or else the index will not be incremented, resulting in an infinite loop.
 * Unwanted elements can be skipped with JsonArray::skip_value().
 *
 *
 * Positional Access
 * -----------------
 *
 *     JsonArray ja = jo.get_array("xydata");
 *     point xydata(ja.get_int(0), ja.get_int(1));
 *
 * Arrays also provide has_int(index) etc., for positional type testing,
 * and read(index, &container) for automatic deserialization.
 *
 *
 * Automatic Deserialization
 * -------------------------
 *
 * Elements can also be automatically read into compatible containers,
 * such as maps, sets, vectors, and classes implementing JsonDeserializer,
 * using the read_next() and read() methods.
 *
 *     JsonArray ja = jo.get_array("custom_datatype_array");
 *     while (ja.has_more()) {
 *         MyDataType mydata; // MyDataType implementing JsonDeserializer
 *         ja.read_next(mydata);
 *         process(mydata);
 *     }
 */
class JsonArray {
private:
    std::vector<int> positions;
    int start;
    int index;
    int end;
    bool final_separator;
    JsonIn *jsin;
    void verify_index(int i);

public:
    JsonArray(JsonIn &jsin);
    JsonArray(const JsonArray &jsarr);
    JsonArray() : positions(), start(0), index(0), end(0), jsin(NULL) {};
    ~JsonArray() { finish(); }

    void finish(); // move the stream position to the end of the array

    bool has_more(); // true iff more elements may be retrieved with next_*
    int size();
    bool empty();
    std::string str(); // copy array json as string
    void throw_error(std::string err);
    void throw_error(std::string err, int idx );

    // iterative access
    bool next_bool();
    int next_int();
    double next_float();
    std::string next_string();
    JsonArray next_array();
    JsonObject next_object();
    void skip_value(); // ignore whatever is next

    // static access
    bool get_bool(int index);
    int get_int(int index);
    double get_float(int index);
    std::string get_string(int index);
    JsonArray get_array(int index);
    JsonObject get_object(int index);

    // iterative type checking
    bool test_null();
    bool test_bool();
    bool test_number();
    bool test_int() { return test_number(); };
    bool test_float() { return test_number(); };
    bool test_string();
    bool test_array();
    bool test_object();

    // random-access type checking
    bool has_null(int index);
    bool has_bool(int index);
    bool has_number(int index);
    bool has_int(int index) { return has_number(index); };
    bool has_float(int index) { return has_number(index); };
    bool has_string(int index);
    bool has_array(int index);
    bool has_object(int index);

    // iteratively read values by reference
    template <typename T> bool read_next(T &t) {
        verify_index(index);
        jsin->seek(positions[index++]);
        return jsin->read(t);
    }
    // random-access read values by reference
    template <typename T> bool read(int i, T &t) {
        verify_index(i);
        jsin->seek(positions[i]);
        return jsin->read(t);
    }
};


/* JsonSerializer
 * ==============
 *
 * JsonSerializer is an inheritable interface,
 * allowing classes to define how they are to be serialized,
 * and then treated as a basic type for serialization purposes.
 *
 * All a class must to do satisfy this interface,
 * is define a `void serialize(JsonOut&) const` method,
 * which should use the provided JsonOut to write its data as JSON.
 *
 * Methods to output to a std::ostream, or as a std::string,
 * are then automatically provided.
 *
 *     class point : public JsonSerializer {
 *         int x, y;
 *         using JsonSerializer::serialize;
 *         void serialize(JsonOut &jsout) const {
 *             jsout.start_array();
 *             jsout.write(x);
 *             jsout.write(y);
 *             jsout.end_array();
 *         }
 *     }
 */
class JsonSerializer {
public:
    virtual ~JsonSerializer() {}
    virtual void serialize(JsonOut &jsout) const = 0;
    std::string serialize() const {
        std::ostringstream s;
        serialize(s);
        return s.str();
    }
    void serialize(std::ostream &o) const {
        JsonOut jout(o);
        serialize(jout);
    }
};

/* JsonDeserializer
 * ==============
 *
 * JsonDeserializer is an inheritable interface,
 * allowing classes to define how they are to be deserialized,
 * and then treated as a basic type for deserialization purposes.
 *
 * All a class must to do satisfy this interface,
 * is define a `void deserialize(JsonIn&)` method,
 * which should read its data from the provided JsonIn,
 * assuming it to be in the correct form.
 *
 * Methods to read from a std::istream, or a std::string,
 * are then automatically provided.
 *
 *     class point : public JsonDeserializer {
 *         int x, y;
 *         using JsonDeserializer::deserialize;
 *         void deserialize(JsonIn &jsin) {
 *             JsonArray ja = jsin.get_array();
 *             x = ja.get_int(0);
 *             y = ja.get_int(1);
 *         }
 *     }
 */
class JsonDeserializer {
public:
    virtual ~JsonDeserializer() {}
    virtual void deserialize(JsonIn &jsin) = 0;
    void deserialize(const std::string &json_string) {
        std::istringstream s(json_string);
        deserialize(s);
    }
    void deserialize(std::istream &i) {
        JsonIn jin(i);
        deserialize(jin);
    }
};

#endif // _JSON_H_
