import XCTest

final class NexaProcessRecoveryTests: XCTestCase {
    func testTerminateRelaunchRestoresWorkspace() {
        let app = XCUIApplication()
        app.launch()
        XCTAssertTrue(app.waitForExistence(timeout: 15))

        let ready = app.staticTexts["Ready · recovery checked"]
        XCTAssertTrue(
            ready.waitForExistence(timeout: 15),
            "The native session did not finish recovery initialization"
        )
        let save = app.buttons["Save snapshot"]
        XCTAssertTrue(save.waitForExistence(timeout: 10))
        save.tap()
        XCTAssertTrue(
            app.staticTexts["OK"].waitForExistence(timeout: 10),
            "Snapshot save did not report success before termination"
        )

        app.terminate()
        app.launch()

        XCTAssertTrue(
            app.staticTexts["Workspace restored"].waitForExistence(timeout: 20),
            "The saved workspace was not restored after relaunch"
        )
        let recoverySuccess = app.staticTexts.containing(
            NSPredicate(format: "label CONTAINS %@", "Recovery: SUCCESS")
        ).firstMatch
        XCTAssertTrue(
            recoverySuccess.waitForExistence(timeout: 10),
            "The structured recovery report did not confirm success"
        )
    }
}
