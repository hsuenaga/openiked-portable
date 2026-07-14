import Foundation
import Dispatch
/*
 * retain closure. anonymous closures are also supported.
 * Apple's C compiler has ARC support. -fblocks is a kind of
 * C extension not a kind of objC extension.
 *
 * ... but manually retained closure sometime helps our debug.
 */
let swiftPuts : @convention(block) (UnsafePointer<CChar>?) -> CBool = { str in
    guard let str else {
        return false;
    }
    let msg = String(cString:str).trimmingCharacters(in: .newlines)
    DispatchQueue.main.async {
        print("[swiftPuts]: \(msg)")
    }
    return true
}

let swiftError : @convention(block) (CInt, UnsafePointer<CChar>?) -> Void = { num, str in
    guard let str else {
        return
    }
    let message = String(cString: str).trimmingCharacters(in: .newlines)
    DispatchQueue.main.async {
        print("[swiftError]: \(num): \(message)")
    }
    return
}

//
// Construct paths
//
guard let resourcePath = Bundle.main.resourcePath else {
    print("No resource directory.")
    exit(1)
}
let tmpDirectoryURL = FileManager.default.temporaryDirectory

print("Initializing IKE with Swift bridge...")
var ikedConfig = OpenIKEDConfig()
ikedConfig.port = 4500
ikedConfig.configurationFile = strdup("\(resourcePath)/etc/iked/iked.conf")
ikedConfig.controlSocket = strdup("\(tmpDirectoryURL.path)/iked.sock")
ikedConfig.ikedPrivKey = strdup("\(resourcePath)/etc/iked/private/local.key")
ikedConfig.ikedCADir = strdup("\(resourcePath)/etc/iked/ca/")
ikedConfig.ikedCRLDir = strdup("\(resourcePath)/etc/iked/crls/")
ikedConfig.ikedCertDir = strdup("\(resourcePath)/etc/iked/certs/")
ikedConfig.resourcePath = strdup("\(resourcePath)")

//
// signal handler
//
var tear_down = false;
signal(SIGINT, SIG_IGN);
let signalSource: any DispatchSourceSignal = DispatchSource.makeSignalSource(
    signal: SIGINT,
    queue: DispatchQueue.global()
)
signalSource.setEventHandler {
    tear_down = true
}
signalSource.resume()

//
// Initialize OpenIKED instance.
//
_ = withUnsafePointer(to: &ikedConfig) { ptr in
    initIKE(ptr, swiftPuts, swiftError)
}

print("Starting IKE...")
startIKE();
print("IKE started. Waiting for events...")

let timer = DispatchSource.makeTimerSource(queue: .main)
timer.schedule(deadline: .now(), repeating: .seconds(10))
timer.setEventHandler {
    if (tear_down) {
        print("SIGINT received.(\(getpid())).");
        print("Waiting for IKE Threads will be finished...")
        stopIKE();
        print("OK. IKE Threads are joined. exiting.")

        exit(0);
    }
    print("Tick. UI thread is still alive!")
}
timer.resume()
dispatchMain()
// not reached.
