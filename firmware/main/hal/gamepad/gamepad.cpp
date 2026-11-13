#include "gamepad.h"
#include "hid_parser.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
extern "C" {
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "os/os_mbuf.h"
#include "nvs.h"
#include "hal/utils/bleprph/bleprph.h"
}
namespace gamepad {
namespace {
std::recursive_mutex mutex;
Snapshot state;
bool initialized = false, ready = false, autoReconnect = true, wantConnection = false;
struct Candidate {
    Device device;
    ble_addr_t addr;
};
std::vector<Candidate> found;
ble_addr_t saved{};
bool haveSaved = false;
std::string savedName;
uint16_t connection = BLE_HS_CONN_HANDLE_NONE, serviceStart = 0, serviceEnd = 0, mapHandle = 0;
struct Report {
    uint16_t value, end, cccd = 0, reference = 0;
    uint8_t id = 0, type = 0;
};
std::vector<Report> reports;
std::vector<uint8_t> reportMap;
HidParser parser;
size_t reportIndex    = 0;
bool codexPad         = false;
bool connectionFailed = false;
struct ble_npl_event commandEvent;
struct ble_npl_callout retry;
bool eventsInitialized = false;
enum Command { None, Scan, Cancel, Connect, Forget };
struct Request {
    Command kind;
    ble_addr_t addr{};
    std::string name;
};
std::deque<Request> commands;
ble_addr_t requested{};
std::string requestedName;
int gap(struct ble_gap_event*, void*);
void startScan();
void discover();
void nextReport();
void savePeer()
{
    ble_gap_conn_desc desc{};
    if (ble_gap_conn_find(connection, &desc)) return;
    saved     = desc.peer_id_addr;
    haveSaved = true;
    savedName = state.name;
    nvs_handle_t h;
    if (nvs_open("gamepad", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "peer", &saved, sizeof saved);
        nvs_set_str(h, "name", savedName.c_str());
        nvs_commit(h);
        nvs_close(h);
    }
}
void fail(const char* text)
{
    connectionFailed = true;
    state.connected  = false;
    state.connecting = false;
    state.buttons    = 0;
    state.status     = text;
    parser.clear();
    if (connection != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(connection, BLE_ERR_REM_USER_CONN_TERM);
}
bool accepted(int rc, const char* text)
{
    if (!rc) return true;
    fail(text);
    return false;
}
void beginConnect(const ble_addr_t& addr, const std::string& name)
{
    if (connection != BLE_HS_CONN_HANDLE_NONE) return;
    ble_gap_disc_cancel();
    connectionFailed = false;
    state.scanning   = false;
    state.connecting = true;
    wantConnection   = true;
    state.name       = name;
    state.status     = "Connecting";
    uint8_t own;
    if (ble_hs_id_infer_auto(0, &own) || ble_gap_connect(own, &addr, 10000, nullptr, gap, nullptr)) {
        state.connecting = false;
        state.status     = "Connection failed";
        ble_npl_callout_reset(&retry, ble_npl_time_ms_to_ticks32(5000));
    }
}
int subscribed(uint16_t c, const ble_gatt_error* e, ble_gatt_attr*, void*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (c != connection) return 0;
    if (e->status) {
        fail("Input subscription failed");
        return 0;
    }
    ++reportIndex;
    nextReport();
    return 0;
}
void subscribeCurrent()
{
    auto& r = reports[reportIndex];
    if (r.type != 1 || (!codexPad && !parser.hasReport(r.id))) {
        ++reportIndex;
        nextReport();
        return;
    }
    if (!r.cccd) {
        fail("Input report has no CCCD");
        return;
    }
    uint8_t enable[] = {1, 0};
    accepted(ble_gattc_write_flat(connection, r.cccd, enable, 2, subscribed, nullptr), "Cannot subscribe input");
}
int referenceRead(uint16_t c, const ble_gatt_error* e, ble_gatt_attr* a, void*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (c != connection) return 0;
    uint8_t ref[2];
    if (e->status || !a || OS_MBUF_PKTLEN(a->om) != 2 || os_mbuf_copydata(a->om, 0, 2, ref)) {
        fail("Invalid report reference");
        return 0;
    }
    reports[reportIndex].id   = ref[0];
    reports[reportIndex].type = ref[1];
    subscribeCurrent();
    return 0;
}
int descriptors(uint16_t c, const ble_gatt_error* e, uint16_t, const ble_gatt_dsc* d, void*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (c != connection) return 0;
    auto& r = reports[reportIndex];
    if (!e->status && d) {
        auto u = ble_uuid_u16(&d->uuid.u);
        if (u == 0x2902) r.cccd = d->handle;
        if (u == 0x2908) r.reference = d->handle;
        return 0;
    }
    if (e->status != BLE_HS_EDONE) {
        fail("Descriptor discovery failed");
        return 0;
    }
    if (codexPad) {
        r.id   = 0;
        r.type = 1;
        subscribeCurrent();
        return 0;
    }
    if (!r.reference) {
        fail("Missing HID report reference");
        return 0;
    }
    accepted(ble_gattc_read(connection, r.reference, referenceRead, nullptr), "Cannot read report reference");
    return 0;
}
void nextReport()
{
    if (reportIndex == reports.size()) {
        bool input = false;
        for (auto r : reports) input |= r.type == 1 && r.cccd && (codexPad || parser.hasReport(r.id));
        if (!input) {
            fail("No supported gamepad input");
            return;
        }
        state.connected  = true;
        state.connecting = false;
        state.status     = "Connected";
        savePeer();
        return;
    }
    auto& r = reports[reportIndex];
    if (r.end <= r.value) {
        fail("Missing HID descriptors");
        return;
    }
    accepted(ble_gattc_disc_all_dscs(connection, r.value, r.end, descriptors, nullptr), "Cannot discover descriptors");
}
int mapRead(uint16_t c, const ble_gatt_error* e, ble_gatt_attr* a, void*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (c != connection) return 0;
    if (!e->status && a) {
        size_t n = OS_MBUF_PKTLEN(a->om);
        if (reportMap.size() + n > 4096) {
            fail("HID map too large");
            return BLE_HS_EAPP;
        }
        size_t old = reportMap.size();
        reportMap.resize(old + n);
        os_mbuf_copydata(a->om, 0, n, reportMap.data() + old);
        return 0;
    }
    if (e->status != BLE_HS_EDONE || !parser.parseMap(reportMap.data(), reportMap.size())) {
        fail("Unsupported HID report map");
        return 0;
    }
    reportIndex = 0;
    nextReport();
    return 0;
}
int characteristics(uint16_t c, const ble_gatt_error* e, const ble_gatt_chr* chr, void*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (c != connection) return 0;
    if (!e->status && chr) {
        if (!reports.empty() && !reports.back().end) reports.back().end = chr->def_handle - 1;
        auto u = ble_uuid_u16(&chr->uuid.u);
        if (u == 0x2a4b) mapHandle = chr->val_handle;
        if ((u == (codexPad ? 0xffa1 : 0x2a4d)) && (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) && reports.size() < 16)
            reports.push_back({chr->val_handle, 0});
        return 0;
    }
    if (e->status != BLE_HS_EDONE || (!codexPad && !mapHandle) || reports.empty()) {
        fail("HID input service missing");
        return 0;
    }
    if (!reports.back().end) reports.back().end = serviceEnd;
    if (codexPad) {
        reportIndex = 0;
        nextReport();
        return 0;
    }
    reportMap.clear();
    accepted(ble_gattc_read_long(connection, mapHandle, 0, mapRead, nullptr), "Cannot read HID map");
    return 0;
}
int services(uint16_t c, const ble_gatt_error* e, const ble_gatt_svc* svc, void*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (c != connection) return 0;
    if (!e->status && svc) {
        auto uuid = ble_uuid_u16(&svc->uuid.u);
        if (uuid == 0xffa0 || (uuid == 0x1812 && !codexPad)) {
            codexPad     = uuid == 0xffa0;
            serviceStart = svc->start_handle;
            serviceEnd   = svc->end_handle;
        }
        return 0;
    }
    if (e->status != BLE_HS_EDONE || !serviceStart) {
        fail("No BLE HID service (Classic/private protocol unsupported)");
        return 0;
    }
    if (!codexPad) {
        ble_gap_conn_desc d{};
        if (ble_gap_conn_find(connection, &d) || !d.sec_state.encrypted) {
            accepted(ble_gap_security_initiate(connection), "Pairing failed");
            return 0;
        }
    }
    accepted(ble_gattc_disc_all_chrs(connection, serviceStart, serviceEnd, characteristics, nullptr),
             "Cannot discover gamepad input");
    return 0;
}
void discover()
{
    serviceStart = serviceEnd = mapHandle = 0;
    codexPad                              = false;
    reports.clear();
    parser.clear();
    accepted(ble_gattc_disc_all_svcs(connection, services, nullptr), "Cannot discover gamepad service");
}
void retryCallback(struct ble_npl_event*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (ready && autoReconnect && haveSaved && connection == BLE_HS_CONN_HANDLE_NONE && !state.scanning &&
        !state.connecting)
        beginConnect(saved, savedName);
}
int gap(struct ble_gap_event* event, void*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            ble_hs_adv_fields fields{};
            if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data)) break;
            if (haveSaved && autoReconnect && ble_addr_cmp(&saved, &event->disc.addr) == 0) {
                beginConnect(event->disc.addr, savedName);
                break;
            }
            bool hid = false;
            for (unsigned i = 0; i < fields.num_uuids16; ++i) hid |= ble_uuid_u16(&fields.uuids16[i].u) == 0x1812;
            bool known = std::any_of(found.begin(), found.end(),
                                     [&](const auto& d) { return ble_addr_cmp(&d.addr, &event->disc.addr) == 0; });
            if (!hid && !known && event->disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
                event->disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND)
                break;
            char id[24];
            std::snprintf(id, sizeof id, "%u:%02x%02x%02x%02x%02x%02x", event->disc.addr.type, event->disc.addr.val[5],
                          event->disc.addr.val[4], event->disc.addr.val[3], event->disc.addr.val[2],
                          event->disc.addr.val[1], event->disc.addr.val[0]);
            std::string name = fields.name ? std::string(reinterpret_cast<const char*>(fields.name),
                                                         std::min<unsigned>(fields.name_len, 48))
                                           : (hid ? "BLE HID Gamepad" : "BLE device (HID unverified)");
            auto it          = std::find_if(found.begin(), found.end(), [&](auto& d) { return d.device.id == id; });
            if (it == found.end() && found.size() < 24)
                found.push_back({{id, name}, event->disc.addr});
            else if (it != found.end() && fields.name)
                it->device.name = name;
            break;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            state.scanning = false;
            if (!state.connecting) state.status = "Scan complete";
            if (autoReconnect && haveSaved) ble_npl_callout_reset(&retry, ble_npl_time_ms_to_ticks32(5000));
            break;
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status) {
                state.connecting = false;
                state.status     = "Connection failed";
                ble_npl_callout_reset(&retry, ble_npl_time_ms_to_ticks32(5000));
                break;
            }
            if (!wantConnection) {
                ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                break;
            }
            connection   = event->connect.conn_handle;
            state.status = "Discovering gamepad";
            discover();
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.conn_handle != connection) break;
            if (event->enc_change.status)
                fail("Encryption failed; forget and pair again");
            else {
                ble_gap_conn_desc d{};
                if (ble_gap_conn_find(connection, &d) || !d.sec_state.encrypted)
                    fail("Encryption required");
                else
                    discover();
            }
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            connectionFailed = true;
            state.status = "Forget device before pairing again";
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        case BLE_GAP_EVENT_DISCONNECT:
            if (event->disconnect.conn.conn_handle != connection) break;
            connection       = BLE_HS_CONN_HANDLE_NONE;
            state.connected  = false;
            state.connecting = false;
            state.buttons    = 0;
            parser.clear();
            // Local discovery/security errors cause a disconnect too. Keep the
            // actionable reason until beginConnect() starts the next attempt.
            if (!connectionFailed) state.status = "Disconnected";
            if (autoReconnect && haveSaved) ble_npl_callout_reset(&retry, ble_npl_time_ms_to_ticks32(5000));
            break;
        case BLE_GAP_EVENT_NOTIFY_RX: {
            if (event->notify_rx.conn_handle != connection || !state.connected) break;
            auto it = std::find_if(reports.begin(), reports.end(),
                                   [&](auto& r) { return r.value == event->notify_rx.attr_handle && r.type == 1; });
            if (it == reports.end()) break;
            uint8_t bytes[512];
            size_t n = OS_MBUF_PKTLEN(event->notify_rx.om);
            if (n <= sizeof bytes && !os_mbuf_copydata(event->notify_rx.om, 0, n, bytes)) {
                if (codexPad)
                    decodeCodexPad(bytes, n, state.buttons);
                else if (parser.input(it->id, bytes, n))
                    state.buttons = parser.buttons();
            }
            break;
        }
        default:
            break;
    }
    return 0;
}
void startScan()
{
    if (connection != BLE_HS_CONN_HANDLE_NONE || state.connecting) return;
    uint8_t own;
    if (ble_hs_id_infer_auto(0, &own)) return;
    ble_gap_disc_params params{};
    params.passive           = 0;
    params.filter_duplicates = 0;
    int rc                   = ble_gap_disc(own, 10000, &params, gap, nullptr);
    state.scanning           = rc == 0 || rc == BLE_HS_EALREADY;
    state.status             = state.scanning ? "Scanning BLE gamepads" : "Scan failed";
}
void execute(struct ble_npl_event*)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (commands.empty()) return;
    Request request = commands.front();
    commands.pop_front();
    Command op = request.kind;
    if (op == Scan && connection == BLE_HS_CONN_HANDLE_NONE && !state.connecting) {
        autoReconnect = false;
        found.clear();
        startScan();
    }
    if (op == Cancel) {
        if (state.scanning && connection == BLE_HS_CONN_HANDLE_NONE && !state.connecting) autoReconnect = false;
        ble_gap_disc_cancel();
        state.scanning = false;
        state.status   = "Scan cancelled";
    }
    if (op == Connect) {
        autoReconnect = true;
        beginConnect(request.addr, request.name);
    }
    if (op == Forget) {
        wantConnection = false;
        autoReconnect  = false;
        ble_npl_callout_stop(&retry);
        ble_gap_disc_cancel();
        ble_gap_conn_cancel();
        state.scanning   = false;
        state.connecting = false;
        state.buttons    = 0;
        state.connected  = false;
        parser.clear();
        if (connection != BLE_HS_CONN_HANDLE_NONE) {
            ble_gap_conn_desc d{};
            if (!ble_gap_conn_find(connection, &d)) ble_store_util_delete_peer(&d.peer_id_addr);
            uint16_t closing = connection;
            connection       = BLE_HS_CONN_HANDLE_NONE;
            ble_gap_terminate(closing, BLE_ERR_REM_USER_CONN_TERM);
        }
        if (haveSaved) ble_store_util_delete_peer(&saved);
        haveSaved = false;
        savedName.clear();
        nvs_handle_t h;
        if (nvs_open("gamepad", NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_all(h);
            nvs_commit(h);
            nvs_close(h);
        }
        state.name.clear();
        state.status = "Forgotten";
    }
    if (!commands.empty()) ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &commandEvent);
}
bool enqueue(Command op)
{
    // Forget must never be lost to a later button press. It also supersedes
    // older pending scan/connect requests before they reach the host.
    if (op == Forget) commands.clear();
    if (commands.size() >= 8) return false;
    commands.push_back({op, requested, requestedName});
    if (ready) ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &commandEvent);
    return true;
}
void reset()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    ready           = false;
    connection      = BLE_HS_CONN_HANDLE_NONE;
    state.connected = state.connecting = state.scanning = false;
    state.buttons                                       = 0;
    parser.clear();
    state.status = "BLE restarting";
}
void synced()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!eventsInitialized) {
        ble_npl_event_init(&commandEvent, execute, nullptr);
        ble_npl_callout_init(&retry, nimble_port_get_dflt_eventq(), retryCallback, nullptr);
        eventsInitialized = true;
    }
    ready = true;
    if (!commands.empty())
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &commandEvent);
    else if (haveSaved && autoReconnect)
        beginConnect(saved, savedName);
}
}  // namespace
void initialize()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (initialized) return;
    initialized  = true;
    state.status = "BLE starting";
    nvs_handle_t h;
    if (nvs_open("gamepad", NVS_READONLY, &h) == ESP_OK) {
        size_t n  = sizeof saved;
        haveSaved = nvs_get_blob(h, "peer", &saved, &n) == ESP_OK && n == sizeof saved;
        char name[64]{};
        n = sizeof name;
        if (nvs_get_str(h, "name", name, &n) == ESP_OK) savedName = name;
        nvs_close(h);
    }
    ble_prph_set_sync_hook(synced);
    ble_prph_set_reset_hook(reset);
    if (ble_prph_is_synced()) synced();
}
void scan()
{
    initialize();
    std::lock_guard<std::recursive_mutex> lock(mutex);
    enqueue(Scan);
}
void cancel_scan()
{
    initialize();
    std::lock_guard<std::recursive_mutex> lock(mutex);
    enqueue(Cancel);
}
std::vector<Device> devices()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::vector<Device> result;
    for (auto& d : found) result.push_back(d.device);
    return result;
}
bool connect(const std::string& id)
{
    initialize();
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (state.connected || state.connecting || std::any_of(commands.begin(), commands.end(), [](const auto& r) {
            return r.kind == Forget || r.kind == Connect;
        }))
        return false;
    auto it = std::find_if(found.begin(), found.end(), [&](auto& d) { return d.device.id == id; });
    if (it == found.end()) return false;
    requested     = it->addr;
    requestedName = it->device.name;
    return enqueue(Connect);
}
void forget()
{
    initialize();
    std::lock_guard<std::recursive_mutex> lock(mutex);
    enqueue(Forget);
}
Snapshot snapshot()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return state;
}
}  // namespace gamepad
