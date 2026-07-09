import Foundation
import Dispatch
/*
 * retain closure. anonymous clousres are also supported.
 * Apple's C compiler has ARC support. -fblocks is a kind of
 * C extenation not a kind of objC extension.
 *
 * ... but manually retained closure sometime helps our debug.
 */
let swiftPuts = { (_ string: UnsafePointer<CChar>?) -> CBool in
    guard let string else {
        return false
    }
    let msg = String(cString:string)
    print("swiftPuts: \(msg)")
    return true
}

let swiftVprintf = { (_ string: UnsafePointer<CChar>?, _ va: CVaListPointer?) -> CInt in
    guard let string, let va else {
        return -1
    }
    let fmt = String(cString: string)
    let message = NSString(format: fmt, arguments: va) as String
    print("swiftVprintf: \(message)")
    return CInt(message.count)
}

let swiftError = { (_ num: CInt, _ string: UnsafePointer<CChar>?) -> Void in
    guard let string else {
        return
    }
    let message = String(cString: string)
    print("swiftError: \(num): \(message)")
    return
}
print("Initializing IKE with Swift bridge...")
//initIKE(swiftVprintf, swiftPuts, swiftError)
initIKE(nil, nil, nil)
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