import Foundation
import Dispatch
/*
 * retain closure. anonymous closures are also supported.
 * Apple's C compiler has ARC support. -fblocks is a kind of
 * C extension not a kind of objC extension.
 *
 * ... but manually retained closure sometime helps our debug.
 */
let swiftPuts = { (_ string: UnsafePointer<CChar>?) -> CBool in
    guard let string else {
        return false
    }
    let msg = String(cString:string)
    DispatchQueue.main.async {
        print("swiftPuts: \(msg)")
    }
    return true
}

let swiftVprintf = { (_ string: UnsafePointer<CChar>?, _ va: CVaListPointer?) -> CInt in
    guard let string, let va else {
        return -1
    }
    let fmt = String(cString: string)
    let message = NSString(format: fmt, arguments: va) as String
    DispatchQueue.main.async {
        print("swiftVprintf: \(message)")
    }
    return CInt(message.count)
}

let swiftError = { (_ num: CInt, _ string: UnsafePointer<CChar>?) -> Void in
    guard let string else {
        return
    }
    let message = String(cString: string)
    DispatchQueue.main.async {
        print("swiftError: \(num): \(message)")
    }
    return
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
let ctrlSock = "\(tmpDirectoryURL.path)/iked.sock"

print("Initializing IKE with Swift bridge...")
initIKE(swiftVprintf, swiftPuts, swiftError, ctrlSock, nil)
//initIKE(nil, nil, nil, "\(tmpDirectoryURL.path)/iked.sock", nil)
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
