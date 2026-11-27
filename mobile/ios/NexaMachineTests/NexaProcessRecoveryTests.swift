import XCTest

final class NexaProcessRecoveryTests: XCTestCase {
    func testTerminateRelaunchRestoresWorkspace() {
        let app = XCUIApplication()
        app.launch()
        XCTAssertTrue(app.waitForExistence(timeout: 15))
        let save = app.buttons["Save snapshot"]
        XCTAssertTrue(save.waitForExistence(timeout: 10))
        save.tap()
        app.terminate()
        app.launch()
        XCTAssertTrue(app.staticTexts["Workspace restored"].waitForExistence(timeout: 20))
        XCTAssertTrue(app.staticTexts.containing(NSPredicate(format: "label CONTAINS %@", "Recovery: SUCCESS")).firstMatch.waitForExistence(timeout: 10))
    }
}
