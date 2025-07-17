<!-- # Startup-Demos/CV_VR/AI_PC

This repository contains various demos and use cases for AI PCs and related technologies. Below is an overview of the sections included in this repository. For detailed information, please refer to the respective subsections in the [AI_PC Hardware Documentation](../../Hardware/AI_PC.md).

## Table of Contents
1. [Introduction to AI PC](#introduction-to-ai-pc)
2. [Snapdragon in Windows Laptops](#snapdragon-in-windows-laptops)
3. [Device Availability](#device-availability)
4. [Marketplace](#marketplace)

## Introduction to AI PC
Learn about the concept of AI PCs, their significance, and how they are transforming the computing landscape. For more details, visit the [Introduction to AI PC](../../Hardware/AI_PC.md#introduction-to-ai-pc) section.

## Snapdragon in Windows Laptops
Explore the integration of Snapdragon processors in Windows laptops, their benefits, and performance metrics. For more details, visit the [Snapdragon in Windows Laptops](../../Hardware/AI_PC.md#snapdragon-in-windows-laptops) section.

## Device Availability
Find out about the availability of AI PC devices in the market, including various models and their specifications. For more details, visit the [Device Availability](../../Hardware/AI_PC.md#device-availability) section.

## Marketplace
Discover the marketplace for AI PC devices, including where to purchase them and the latest trends. For more details, visit the [Marketplace](../../Hardware/AI_PC.md#marketplace) section. -->

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>DailyVision - README</title>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; margin: 40px; }
        h1, h2, h3 { color: #2c3e50; }
        code { background-color: #f4f4f4; padding: 2px 4px; border-radius: 4px; }
        pre { background-color: #f4f4f4; padding: 10px; border-radius: 4px; overflow-x: auto; }
        ul { margin-top: 0; }
    </style>
</head>
<body>

<h1>DailyVision 🚦🧠</h1>
<p><strong>Team QVision</strong> | Edge AI Hackathon Project</p>

<p>DailyVision is an Edge AI-powered assistive vision system designed to detect traffic lights and signboards in real-time, extract relevant text, and provide voice feedback to the user. It is optimized for low-power edge devices and integrates object detection, OCR, and text-to-speech technologies.</p>

<h2>🚀 Project Overview</h2>
<p>DailyVision helps users—especially those with visual impairments—understand their surroundings by:</p>
<ul>
    <li>Detecting traffic lights and identifying their color (Red, Yellow, Green)</li>
    <li>Recognizing signboards and extracting text using OCR</li>
    <li>Providing real-time voice feedback using offline TTS</li>
</ul>

<h2>✨ Features</h2>
<ul>
    <li>🔍 <strong>YOLOv8 Object Detection</strong> for identifying traffic lights and signboards</li>
    <li>🧾 <strong>EasyOCR</strong> for extracting text from detected regions</li>
    <li>🗣️ <strong>Text-to-Speech</strong> using <code>pyttsx3</code> for offline voice feedback</li>
    <li>🎨 <strong>Traffic Light Color Classification</strong> using HSV color masks</li>
    <li>🖼️ Annotated image output with bounding boxes and labels</li>
</ul>

<h2>🛠️ Tech Stack</h2>
<ul>
    <li>Python 3</li>
    <li>Ultralytics YOLOv8</li>
    <li>OpenCV</li>
    <li>EasyOCR via <code>qai_hub_models</code></li>
    <li>pyttsx3 (offline TTS)</li>
    <li>NumPy, Pillow</li>
</ul>

<h2>📦 Installation</h2>
<pre><code>git clone https://github.com/1vicvis7/Startup-Demos.git
cd DailyVision
pip install -r requirements.txt
</code></pre>

<h2>▶️ Usage</h2>
<pre><code>python main.py</code></pre>
<p>Make sure to place your test images in the appropriate folder and update the image path in <code>main.py</code>.</p>

<h2>📁 Folder Structure</h2>
<pre><code>DailyVision/
├── main.py
├── requirements.txt
├── images/
│   ├── traffic_red.jpg
│   └── signboard_1.jpg # other images
├── src/
│   ├── audio.py
│   ├── detection.py
│   ├── ocr.py
│   └── utils.py
</code></pre>

<h2>🧪 Sample Output</h2>
<ul>
    <li>Annotated image saved as <code>annotated_output.jpg</code></li>
    <li>Console output:
        <pre><code>Detected a traffic light: with color: red
Detected a signboard: STOP</code></pre>
    </li>
    <li>Voice feedback via speakers</li>
</ul>

<h2>👥 Team</h2>
<p><strong>Team QVision</strong></p>
<ul>
    <li>Vishnudatta – Engineering Intern</li>
    <li>Guna Nekkanti – Engineering Intern</li>
    <li>Sneha Das – Engineering Intern</li>
    <li>Mythreya Garudadri – Engineering Intern</li>
</ul>

<h2>📄 License</h2>
<p>This project is licensed under the MIT License. See the <a href="https://github.com/qualcomm/Startup-Demos/blob/main/LICENSE.txt">LICENSE</a> file for details.</p>

<h2>🏁 Acknowledgements</h2>
<ul>
    <li>Qualcomm AI Hub</li>
    <li>Ultralytics YOLOv8</li>
    <li>EasyOCR</li>
</ul>

</body>
</html>
