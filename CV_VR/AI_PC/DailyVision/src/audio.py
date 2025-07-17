# //===------DailyVision-src\audio.py-----------------------------------------===//
# // Part of the Startup-Demos Project, under the MIT License
# // See https://github.com/qualcomm/Startup-Demos/blob/main/LICENSE.txt
# // for license information.
# // Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# // SPDX-License-Identifier: MIT License
# // Project done by - Vishnudatta (vindraga@qti.qualcomm.com)
# //===----------------------------------------------------------------------===//

import pyttsx3

def speak_text(text):
    speaker = pyttsx3.init()
    speaker.say(text)
    speaker.runAndWait()
