# 🌱 NurseryMate

**NurseryMate** is an intuitive inventory management app designed specifically for ornamental plant nurseries. It helps nursery owners track their stock, manage purchase histories, and monitor inventory levels in real-time. Whether you're growing your nursery business or just want to stay organized, PlantStock gives you the tools to manage your stock efficiently.

## 🚀 Features

- **Product Search & Filter**: Easily search and filter through your products to find specific plants or items.
- **Purchase History**: Track purchase details including suppliers, purchase dates, quantities, and frequency.
- **Real-Time Stock Management**: Monitor current stock levels and receive alerts for low stock.
- **Supplier Management**: Manage supplier information and track who supplied each product.
- **Analytics & Reports**: Gain insights on purchasing trends, inventory value, and stock movement.
- **Cross-Platform**: Available on desktop and mobile using Qt’s powerful cross-platform framework.

## 🛠️ Technologies Used

- **Frontend**: Qt Quick (QML) for UI
- **Backend**: C++ for core logic
- **Database**: SQLite (or preferred database for managing stock)
- **Other**: QCharts (for analytics and reports)

## 🛠️ Installation

1. Clone the repository:

   ```bash
   git clone https://github.com/yourusername/NurseryMate.git
   cd NurseryMate

2. Open the project in **Qt Creator** or build manually using **CMake**:

   **In Qt Creator**:
   - Open Qt Creator and go to `File > Open File or Project...`.
   - Select the `CMakeLists.txt` file in the project directory.
   - Click "Configure Project" and follow the prompts to set up the build environment.
   - Once configured, click the "Run" button to build and run the app.

   **Building manually using CMake**:
   - Navigate to the project directory:
     ```bash
     cd NurseryMate
     ```
   - Create a build directory and configure the project:
     ```bash
     mkdir build
     cd build
     cmake ..
     ```
   - Build the project:
     ```bash
     make
     ```
   
3. Run the application:

   ```bash
   ./NurseryMate

4. The app should now be running on your desktop, ready to help you manage your plant nursery inventory.

## 🌿 How to Use

1. **Add Products**: Start by adding your nursery products into the system.
2. **Track Purchases**: Add purchase details to monitor stock and supplier information.
3. **Monitor Stock**: Keep an eye on stock levels, and receive notifications when products run low.
4. **Generate Reports**: Use the analytics feature to track stock trends and gain insights into your inventory.

## 🎨 Future Enhancements

- **Barcode/QR Code Integration**: For quick product search and inventory updates.
- **Accounting Software Integration**: To track stock-related expenses automatically.
- **User Roles & Permissions**: Different access levels for employees to manage stock or view reports.

## 👥 Contributing

We welcome contributions! If you'd like to contribute to PlantStock, please follow these steps:

1. Fork the repository.
2. Create a new branch:
   ```bash
   git checkout -b feature/your-feature
3. Make your changes and commit them:
   ```bash
   git commit -m "Add feature"
4. Push to the branch:
   ```bash
   git push origin feature/your-feature
5. Submit a pull request to the main repository.

## 📄 License

This project is licensed under the MIT License. 

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

**THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND**, express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose and noninfringement. In no event shall the authors or copyright holders be liable for any claim, damages or other liability, whether in an action of contract, tort or otherwise, arising from, out of or in connection with the Software or the use or other dealings in the Software.
