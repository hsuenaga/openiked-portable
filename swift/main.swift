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

guard let resourcePath = Bundle.main.resourcePath else {
    print("No resource directory.")
    exit(1)
}

func getApplicationSupportDirectory() -> URL? {
    do {
        // Passing 'create: true' ensures the directory is built if it doesn't exist
        let appSupportURL = try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        )
        return appSupportURL
    } catch {
        print("Error locating Application Support Directory: \(error.localizedDescription)")
        return nil
    }
}

// Usage
if let supportDir = getApplicationSupportDirectory() {
    print("Application Support Path: \(supportDir.path)")
}

let tmpDirectoryURL = FileManager.default.temporaryDirectory
print("tmp: \(tmpDirectoryURL)")
print("resource: \(resourcePath)")
let ctrlSock = "\(tmpDirectoryURL.path)/iked.sock"
let configFile = "\(resourcePath)/etc/iked/iked.conf"

print("Initializing IKE with Swift bridge...")
var ikedConfig = OpenIKEDConfig()
withUnsafePointer(to: &ikedConfig) { ptr in
    initIKE(ptr, swiftPuts, swiftError)
}

print("Starting IKE...")
startIKE();
print("IKE started. Waiting for events...")

let timer = DispatchSource.makeTimerSource(queue: .main)
timer.schedule(deadline: .now(), repeating: .seconds(1))
timer.setEventHandler {
    print("tick")
}
timer.resume()
dispatchMain()

print("Exiting IKE...")
