module.exports = async function () {
  const pid = parseInt(process.env.TEST_SERVER_PID, 10);
  if (pid) {
    try {
      process.kill(pid);
      console.log(`\nTest server (PID ${pid}) stopped`);
    } catch {
      // Already exited
    }
  }
};
