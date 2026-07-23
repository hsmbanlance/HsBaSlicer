/**
 * @file ViewController.swift
 * @brief HsBaSlicer iOS 示例入口
 *
 * 演示如何在 iOS App 中调用 HsBaSlicer 流水线示例。
 * 通过 Bridging Header 调用 C 函数 HsBaRunPipelineExamples()。
 *
 * 注意：runPipelineExamples() 为非生产入口，仅供示例和测试使用。
 * 生产环境应在后台队列执行流水线任务。
 */

import UIKit

class ViewController: UIViewController {

    private let statusLabel: UILabel = {
        let label = UILabel()
        label.textAlignment = .center
        label.numberOfLines = 0
        label.text = "HsBaSlicer native libs loaded\nTap to run pipeline examples..."
        return label
    }()

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white

        // 布局状态标签
        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(statusLabel)
        NSLayoutConstraint.activate([
            statusLabel.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            statusLabel.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            statusLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 20),
            statusLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20)
        ])

        // 添加点击手势触发示例
        let tapGesture = UITapGestureRecognizer(target: self, action: #selector(runExamples))
        view.addGestureRecognizer(tapGesture)
    }

    /// 在后台队列运行流水线示例（仅供测试，避免阻塞 UI）
    @objc private func runExamples() {
        statusLabel.text = "Running pipeline examples..."

        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            // 调用 C++ 导出的流水线示例函数
            HsBaRunPipelineExamples()

            DispatchQueue.main.async {
                self?.statusLabel.text = "HsBaSlicer pipeline examples completed successfully."
            }
        }
    }
}

// ---------------------------------------------------------------------------
// App 入口（SwiftUI lifecycle 或传统 AppDelegate）
// ---------------------------------------------------------------------------
@main
struct HsBaSlicerExampleApp {
    static func main() {
        UIApplicationMain(
            CommandLine.argc,
            CommandLine.unsafeArgv,
            nil,
            NSStringFromClass(AppDelegate.self)
        )
    }
}

class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
    ) -> Bool {
        window = UIWindow(frame: UIScreen.main.bounds)
        window?.rootViewController = ViewController()
        window?.makeKeyAndVisible()
        return true
    }
}
