#define PY_SSIZE_T_CLEAN
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <csignal>
#include <filesystem>
#include <tuple>
#include <unistd.h>
#include <libvirt.h>
#include <Python.h>
namespace fs = std::filesystem;

sig_atomic_t sigRecv = 0;

void check_args(int argc, char *argv[]) {

    if (argc != 2 ) {
        std::cerr << "FATAL ERROR: INCORRECT USAGE\n\
Use with one argument as absolute path \
to domain xml: virsh-persist </absolute/path/to/domain/xml.xml>" << std::endl;
        exit(EXIT_FAILURE);
    }

    fs::path domainPath{ argv[1] };
    fs::file_status status = fs::status(domainPath);

    if (fs::status_known(status) ? !fs::exists(status) : !fs::exists(domainPath)) {
        std::cerr << "FATAL ERROR: FILE NOT FOUND\n\
The given file could not be found. \
If it truly does exist, make sure the user this program is being run as \
has read permissions to the file. If on a machine with SELINUX enabled, \
check for possible policy denials." << std::endl;
        exit(EXIT_FAILURE);
    }

    return;

}

std::tuple<std::string, std::string> read_domain_xml(char *argv[]) {

    const char *domainName;
    PyObject *pyValue, *pyModule, *pyDict;

    Py_Initialize();

    pyModule = PyImport_AddModule("__main__");
    pyDict = PyModule_GetDict(pyModule);

    std::string tree("tree = ET.parse(open('");
    std::string str(argv[1]);
    std::string tree2("'))");
    tree = tree + argv[1] + tree2;

    PyRun_SimpleString("import xml.etree.ElementTree as ET");
    PyRun_SimpleString(tree.c_str());
    PyRun_SimpleString("root = tree.getroot()");
    pyValue = PyRun_String("root.find('name').text", Py_eval_input, pyDict, pyDict);

    domainName = PyUnicode_AsUTF8(pyValue);

    if (Py_FinalizeEx() < 0) {
        exit(120);
    }

    std::ifstream input_file(argv[1]);
    return {domainName, std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>())};

}

virDomainPtr define_domain(virConnectPtr conn, virDomainPtr dom, std::string domainXML) {

    if (!dom) {
        dom = virDomainDefineXML(conn, domainXML.c_str());
        if (!dom) {
            std::cerr << "Unable to define persistent guest configuration" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    return dom;

}

virDomainPtr start_domain(virDomainPtr dom, virDomainInfoPtr info) {

    virDomainGetInfo(dom, info);

    if (info->state == VIR_DOMAIN_NOSTATE || 
        info->state == VIR_DOMAIN_PAUSED || 
        info->state == VIR_DOMAIN_SHUTOFF || 
        info->state == VIR_DOMAIN_CRASHED || 
        info->state == VIR_DOMAIN_PMSUSPENDED) {
        if (virDomainCreate(dom) < 0) {
            virDomainFree(dom);
            std::cerr << "Cannot boot guest" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    return dom;

}

void cleanup(virConnectPtr conn, virDomainPtr dom, virDomainInfoPtr info) {

    int shutdown;

    shutdown  = virDomainShutdown(dom);
    virDomainGetInfo(dom, info);

    if (shutdown < 0 || info->state != VIR_DOMAIN_SHUTDOWN || info->state != VIR_DOMAIN_SHUTOFF) {
        virDomainDestroyFlags(dom, VIR_DOMAIN_DESTROY_GRACEFUL);
    }

    virDomainFree(dom);
    virConnectClose(conn);

    exit(sigRecv);

}

void signalHandler(int signum) {

    sigRecv = signum;

}

int main(int argc, char *argv[]) {

    std::tuple<std::string, std::string> tup;
    std::string domainName;
    std::string domainXML;
    virConnectPtr conn;
    virDomainPtr dom;
    virDomainInfoPtr info;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGABRT, signalHandler);

    check_args(argc, argv);
    tup = read_domain_xml(argv);
    domainName = std::get<0>(tup);
    domainXML = std::get<1>(tup);

    conn = virConnectOpen("qemu:///system");
    if (conn == NULL) {
        std::cerr << "Failed to open connection to qemu:///system" << std::endl;
        return 1;
    }

    dom = virDomainLookupByName(conn, domainName.c_str());

    while (1) {
        dom = define_domain(conn, dom, domainXML);
        dom = start_domain(dom, info);
        sleep(5);
        if (sigRecv) {
            cleanup(conn, dom, info);
        }
    }

    return 0;
}