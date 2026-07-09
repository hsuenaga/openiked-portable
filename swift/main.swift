import Foundation

print("Hello, World")

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
    DispatchQueue.main.async {
        print("swiftPuts: \(msg)")
    }
    return true
}

let swiftPrintf = { (_ string: UnsafePointer<CChar>?, _ va: CVaListPointer?) -> CInt in
    guard let string, let va else {
        return -1
    }
    let fmt = String(cString: string)
    let message = NSString(format: fmt, arguments: va) as String
    DispatchQueue.main.async {
        print("swiftPrintf: \(message)")
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

initIKE(swiftPrintf, swiftPuts, swiftError)
startIKE();
