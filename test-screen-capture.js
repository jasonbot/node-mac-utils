const macUtils = require("./index.js");

console.log("Testing Screen Capture Permission APIs");
console.log("========================================\n");

// Test checkScreenCaptureAccess
console.log("1. Checking current screen capture access status...");
const hasAccess = macUtils.checkScreenCaptureAccess();
console.log(`   Current status: ${hasAccess ? "GRANTED" : "NOT GRANTED"}\n`);

// If we don't have access, request it
if (!hasAccess) {
  console.log("2. Requesting screen capture access...");
  const granted = macUtils.requestScreenCaptureAccess();
  console.log(`   Request result: ${granted ? "GRANTED" : "DENIED"}\n`);

  if (!granted) {
    console.log("⚠️  Screen capture access was not granted.");
    console.log("   Please grant permission in System Preferences > Privacy & Security > Screen Recording\n");
  } else {
    console.log("✅ Screen capture access granted!\n");
  }
} else {
  console.log("✅ Screen capture access already granted!\n");
}

// Test again to confirm
console.log("3. Checking screen capture access status again...");
const finalStatus = macUtils.checkScreenCaptureAccess();
console.log(`   Final status: ${finalStatus ? "GRANTED" : "NOT GRANTED"}\n`);
