#include "common/file.h"

#include <gtest/gtest.h>
#include <gflags/gflags.h>

#include <sstream>
#include <algorithm>
#include <cstdlib>

using namespace baidu::shuttle;

/*
 * Test needs an address to an inexist location, and will automatically create testcase file
 *   and destroy them after all test if tests run as ordered
 * User must manually delete left test file when:
 *   1. Random call all there testcases: Some testcases may not clean the test file
 *   2. Meet assertions: Test file WILL NOT be destroyed when test environment tears down
 */

DEFINE_string(address, "", "full address to the test file");
DEFINE_string(user, "", "username to FS, empty means default");
DEFINE_string(password, "", "password to FS, empty only when username is empty");

// Global file pointer for FileIOTest, will be automatically initialized and released
static File* fp = NULL;
// Path information in address. Initialized when FileIOTest environments set up
static std::string path;

void FillParam(File::Param& param) {
    std::string host, port;
    if (!File::ParseFullAddress(FLAGS_address, NULL, &host, &port, NULL)) {
        host = "";
        port = "";
    }
    if (host != "") {
        param["host"] = host;
    }
    if (port != "") {
        param["port"] = port;
    }
    if (FLAGS_user != "") {
        param["user"] = FLAGS_user;
    }
    if (FLAGS_password != "") {
        param["password"] = FLAGS_password;
    }
}

/*
 * This test checks the connectivity to the HDFS
 *   Need to manually check if the connectivity is same with the output
 *   Tests rely on ParseFullAddress
 */
TEST(FileToolsTest, ConnectInfHdfs) {
    File::Param param;
    FillParam(param);
    void* fs = NULL;
    ASSERT_TRUE(File::ConnectInfHdfs(param, &fs));
    ASSERT_TRUE(fs != NULL);
}

// BuildParam interface relies on ParseFullAddress
TEST(FileToolsTest, BuildParamTest) {
    DfsInfo info;
    const File::Param& param0 = File::BuildParam(info);
    EXPECT_TRUE(param0.find("host") == param0.end());
    EXPECT_TRUE(param0.find("port") == param0.end());
    EXPECT_TRUE(param0.find("user") == param0.end());
    EXPECT_TRUE(param0.find("password") == param0.end());

    info.set_host("localhost");
    info.set_port("9999");
    const File::Param& param1 = File::BuildParam(info);
    EXPECT_TRUE(param1.find("host") != param1.end());
    EXPECT_EQ(param1.find("host")->second, "localhost");
    EXPECT_TRUE(param1.find("port") != param1.end());
    EXPECT_EQ(param1.find("port")->second, "9999");
    EXPECT_TRUE(param1.find("user") == param1.end());
    EXPECT_TRUE(param1.find("password") == param1.end());

    info.set_user("me");
    info.set_password("password");
    const File::Param& param2 = File::BuildParam(info);
    EXPECT_EQ(param2.find("host")->second, "localhost");
    EXPECT_EQ(param2.find("port")->second, "9999");
    EXPECT_TRUE(param2.find("user") != param2.end());
    EXPECT_EQ(param2.find("user")->second, "me");
    EXPECT_TRUE(param2.find("password") != param2.end());
    EXPECT_EQ(param2.find("password")->second, "password");

    info.set_path("hdfs://0.0.0.0:6666/whatever/file/is.file");
    const File::Param& param3 = File::BuildParam(info);
    EXPECT_TRUE(param3.find("host") != param3.end());
    EXPECT_EQ(param3.find("host")->second, "0.0.0.0");
    EXPECT_TRUE(param3.find("port") != param3.end());
    EXPECT_EQ(param3.find("port")->second, "6666");
}

TEST(FileToolsTest, ParseAddressTest) {
    // --- HDFS format test ---
    std::string address = "hdfs://localhost:9999/home/test/hdfs.file";
    std::string host, port, path;
    FileType type = kLocalFs;
    EXPECT_TRUE(File::ParseFullAddress(address, &type, &host, &port, &path));
    EXPECT_EQ(type, kInfHdfs);
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, "9999");
    EXPECT_EQ(path, "/home/test/hdfs.file");

    type = kLocalFs;
    address = "hdfs://0.0.0.0:/no/port/test.file";
    EXPECT_TRUE(File::ParseFullAddress(address, &type, &host, &port, &path));
    EXPECT_EQ(type, kInfHdfs);
    EXPECT_EQ(host, "0.0.0.0");
    EXPECT_EQ(port, "");
    EXPECT_EQ(path, "/no/port/test.file");

    type = kLocalFs;
    address = "hdfs://:/empty/host/test.file";
    EXPECT_TRUE(File::ParseFullAddress(address, &type, &host, &port, &path));
    EXPECT_EQ(type, kInfHdfs);
    EXPECT_EQ(host, "");
    EXPECT_EQ(port, "");
    EXPECT_EQ(path, "/empty/host/test.file");

    type = kLocalFs;
    address = "hdfs://localhost/no/colon/is/okay/test.file";
    EXPECT_TRUE(File::ParseFullAddress(address, &type, &host, &port, &path));
    EXPECT_EQ(type, kInfHdfs);
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, "");
    EXPECT_EQ(path, "/no/colon/is/okay/test.file");

    type = kLocalFs;
    address = "hdfs:///no/host/port/info/test.file";
    EXPECT_TRUE(File::ParseFullAddress(address, &type, &host, &port, &path));
    EXPECT_EQ(type, kInfHdfs);
    EXPECT_EQ(host, "");
    EXPECT_EQ(port, "");
    EXPECT_EQ(path, "/no/host/port/info/test.file");

    // --- Local format test ---
    type = kInfHdfs;
    address = "file:///home/test/local.file";
    EXPECT_TRUE(File::ParseFullAddress(address, &type, &host, &port, &path));
    EXPECT_EQ(type, kLocalFs);
    EXPECT_EQ(host, "");
    EXPECT_EQ(port, "");
    EXPECT_EQ(path, "/home/test/local.file");

    // Acceptable
    type = kInfHdfs;
    address = "file://localhost:80/local/with/host/test.file";
    EXPECT_TRUE(File::ParseFullAddress(address, &type, &host, &port, &path));
    EXPECT_EQ(type, kLocalFs);
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, "80");
    EXPECT_EQ(path, "/local/with/host/test.file");

    // --- Invalid format ---
    address = "dfs://localhost:9999/format/is/invalid/test.file";
    EXPECT_TRUE(!File::ParseFullAddress(address, &type, &host, &port, &path));

    address = "";
    EXPECT_TRUE(!File::ParseFullAddress(address, &type, &host, &port, &path));
}

TEST(FileToolsTest, PatternMatchTest) {
    // --- Perfect match test ---
    std::string pattern = "test_string";
    std::string origin = "test_string";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    // --- Star match test ---
    pattern = "*";
    origin = "whatever_the_string_is";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    pattern = "begin_*_end";
    origin = "begin_blahblahblah_end";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    // Check if * will be misled and teminated too soon
    pattern = ">*<";
    origin = ">mislead<test<";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    pattern = "/*/*/*";
    origin = "/multiple/star/match";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    pattern = "/*/*/*";
    origin = "//nothing/there";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    // --- Question mark match test ---
    pattern = "/aha?";
    origin = "/aha!";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    pattern = "/self/match?";
    origin = "/self/match?";
    EXPECT_TRUE(File::PatternMatch(origin, pattern));

    pattern = "/must/have/?/something";
    origin = "/must/have//something";
    EXPECT_TRUE(!File::PatternMatch(origin, pattern));
}

class FileIOTest : public testing::Test {
protected:
    virtual void SetUp() {
        if (fp != NULL) {
            return;
        }
        ASSERT_TRUE(FLAGS_address != "");
        FileType type;
        ASSERT_TRUE(File::ParseFullAddress(FLAGS_address, &type, NULL, NULL, &path));
        File::Param param;
        FillParam(param);
        fp = File::Create(type, param);
        ASSERT_TRUE(fp != NULL);
    }

    virtual void TearDown() {
        if (fp != NULL) {
            delete fp;
        }
        fp = NULL;
    }
};

TEST_F(FileIOTest, OpenCloseNameTest) {
    File::Param param;

    // Create or truncate file
    ASSERT_TRUE(fp->Open(path, kWriteFile, param));
    EXPECT_EQ(fp->GetFileName(), path);
    ASSERT_TRUE(fp->Close());

    ASSERT_TRUE(fp->Open(path, kReadFile, param));
    EXPECT_EQ(fp->GetFileName(), path);
    ASSERT_TRUE(fp->Close());
    // Leave a test file in path
}

TEST_F(FileIOTest, ReadWriteTest) {
    File::Param param;
    ASSERT_TRUE(fp->Open(path, kWriteFile, param));
    std::string write_buf;
    for (int i = 0; i < 100; ++i) {
        write_buf += "this is a test string\n";
    }
    ASSERT_TRUE(fp->WriteAll(write_buf.data(), write_buf.size()));
    ASSERT_TRUE(fp->Close());

    ASSERT_TRUE(fp->Open(path, kReadFile, param));
    size_t read_buf_size = write_buf.size() + 1;
    char* read_buf = new char[read_buf_size];
    size_t read_n = fp->ReadAll(read_buf, read_buf_size);
    std::string read_str;
    read_str.assign(read_buf, read_n);
    EXPECT_EQ(write_buf, read_str);
    ASSERT_TRUE(fp->Close());
    // Leave a test file in path
}

TEST_F(FileIOTest, RenameRemoveExistTest) {
    File::Param param;
    // Create or truncate file
    ASSERT_TRUE(fp->Open(path, kWriteFile, param));
    ASSERT_TRUE(fp->Close());
    // Assertion here may leave a test file

    // Existence of path is garanteed
    ASSERT_TRUE(fp->Exist(path));

    // Rename twice and check existence
    std::string new_path = path + "_test_newfile";
    // In case of overwriting
    ASSERT_TRUE(!fp->Exist(new_path));
    EXPECT_TRUE(fp->Rename(path, new_path));
    EXPECT_TRUE(!fp->Exist(path));
    EXPECT_TRUE(fp->Exist(new_path));
    EXPECT_TRUE(fp->Rename(new_path, path));
    EXPECT_TRUE(!fp->Exist(new_path));
    EXPECT_TRUE(fp->Exist(path));

    // Remove file test
    EXPECT_TRUE(fp->Remove(path));
    EXPECT_TRUE(!fp->Exist(path));

    // Remove directory test
    EXPECT_TRUE(fp->Mkdir(path));
    EXPECT_TRUE(fp->Exist(path));
    EXPECT_TRUE(fp->Remove(path));
    EXPECT_TRUE(!fp->Exist(path));
    // No test file is left
}

TEST_F(FileIOTest, TellSeekTest) {
    // Rebuild a test file
    File::Param param;
    // Create or truncate file. Fail if there's a directory
    ASSERT_TRUE(fp->Open(path, kWriteFile, param));
    std::string write_buf;
    for (int i = 0; i < 100; ++i) {
        write_buf += "this is a test string\n";
    }
    ASSERT_TRUE(fp->WriteAll(write_buf.data(), write_buf.size()));
    ASSERT_TRUE(fp->Close());

    ASSERT_TRUE(fp->Open(path, kReadFile, param));
    size_t size = fp->GetSize();
    EXPECT_TRUE(size > 0);

    EXPECT_EQ(fp->Tell(), 0);
    EXPECT_TRUE(fp->Seek(size >> 1));
    EXPECT_EQ(fp->Tell(), static_cast<int>(size >> 1));

    ASSERT_TRUE(fp->Close());
    // Leave a test file in path
}

struct FileInfoComparator {
    bool operator()(const FileInfo& lhs, const FileInfo& rhs) {
        // If no '/' is found, then find_last_of returns npos, add 1 will be 0
        std::string lns = lhs.name.substr(lhs.name.find_last_of('/') + 1);
        std::string rns = rhs.name.substr(rhs.name.find_last_of('/') + 1);
        return atoi(lns.c_str()) < atoi(rns.c_str());
    }
};

bool isFileInfoEqual(const FileInfo& lhs, const FileInfo& rhs) {
    return lhs.kind == rhs.kind && lhs.name == rhs.name && lhs.size ==rhs.size;
}

TEST_F(FileIOTest, ListGlobTest) {
    // Clean up test path and prepare testcase for list/glob
    // No need to check the return value since path might refers nothing
    fp->Remove(path);
    ASSERT_TRUE(fp->Mkdir(path));
    std::string testdir = *path.rbegin() == '/' ? path : path + '/';
    for (int i = 0; i < 1000; ++i) {
        std::stringstream ss;
        ss << std::setw(4) << std::setfill('0') << i;
        ASSERT_TRUE(fp->Mkdir(testdir + ss.str()));
        // Assertion here may leave files
    }

    std::vector<FileInfo> list_children;
    ASSERT_TRUE(fp->List(path, &list_children));
    std::sort(list_children.begin(), list_children.end(), FileInfoComparator());

    // Check list result
    int size = static_cast<int>(list_children.size());
    ASSERT_EQ(size, 1000);
    for (int i = 0; i < size; ++i) {
        const FileInfo& info = list_children[i];
        EXPECT_EQ(info.kind, 'D');
        std::string file_num = info.name.substr(info.name.find_last_of('/') + 1);
        EXPECT_EQ(atoi(file_num.c_str()), i);
    }

    std::vector<FileInfo> glob_children;
    ASSERT_TRUE(fp->Glob(testdir + '*', &glob_children));
    std::sort(glob_children.begin(), glob_children.end(), FileInfoComparator());

    // Glob should behave the same as list here
    ASSERT_EQ(list_children.size(), glob_children.size());
    EXPECT_TRUE(std::equal(list_children.begin(), list_children.end(),
            glob_children.begin(), isFileInfoEqual));

    for (int i = 0; i < 1000; ++i) {
        std::stringstream ss;
        ss << std::setw(4) << std::setfill('0') << i;
        EXPECT_TRUE(fp->Remove(testdir + ss.str()));
    }
    EXPECT_TRUE(fp->Remove(path));
    // No test file is left
}

TEST(FileHubTest, FileHubSaveLoadTest) {
    // Prepare testcases
    std::vector<File::Param> params;
    std::vector<int*> ints;
    std::string host("test_host");
    FileHub<int>* hub = FileHub<int>::GetHub();
    for (int i = 0; i < 100; ++i) {
        File::Param param;
        std::stringstream ss;
        ss << i;
        param["host"] = host;
        param["port"] = ss.str();
        params.push_back(param);
        int* p = new int;
        *p = i;
        ints.push_back(p);
        hub->Store(param, p);
    }
    // Check all stored pointers and params
    for (int i = 0; i < 100; ++i) {
        std::stringstream ss;
        ss << i;
        int* p = hub->Get(host, ss.str());
        ASSERT_TRUE(p != NULL);
        EXPECT_EQ(*p, i);

        File::Param param = hub->GetParam(host, ss.str());
        EXPECT_EQ(param["host"], host);
        EXPECT_EQ(param["port"], ss.str());
    }
    // Check inexist pointer
    EXPECT_TRUE(hub->Get("unknown-host", "0") == NULL);
    EXPECT_TRUE(hub->Get(host, "invalid-port") == NULL);
    // Hub should automatically delete int pointers since it uses scoped pointer inside
    delete hub;
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    google::ParseCommandLineFlags(&argc, &argv, true);
    return RUN_ALL_TESTS();
}

