#pragma once

// Starts the task that sends scanner status to the website, and
// camera pictures only while someone has the page open. The task
// runs on core 0, so a slow server cannot delay QR scanning.
void uplinkStart();
