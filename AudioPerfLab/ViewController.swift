// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

import UIKit
import os

fileprivate func numProcessors() -> Int {
  return ProcessInfo.processInfo.processorCount
}

class ViewController: UITableViewController {
  private var engine: Engine!
  private var displayLink: CADisplayLink!
  private var coreActivityViews = [ActivityView]()
  private var lastNumFrames: Int32?

  @IBOutlet weak private var activityViewsEnabledSwitch: UISwitch!
  @IBOutlet weak private var driveDurationsView: ActivityView!
  @IBOutlet weak private var coreActivityStackView: UIStackView!
  @IBOutlet weak private var bufferSizeStepper: UIStepper!
  @IBOutlet weak private var bufferSizeField: UITextField!
  @IBOutlet weak private var numSinesSlider: SliderWithValue!
  @IBOutlet weak private var numBurstSinesSlider: SliderWithValue!
  @IBOutlet weak private var numThreadsSlider: SliderWithValue!
  @IBOutlet weak private var minimumLoadSlider: SliderWithValue!
  @IBOutlet weak private var numBusyThreadsSlider: SliderWithValue!
  @IBOutlet weak private var isWorkIntervalOnSwitch: UISwitch!

  private static let activityViewDuration = 3.0
  private static let activityViewLatency = 0.1
  private static let dropoutColor = UIColor.red
  private static let threadColors = [
    UIColor.black,
    UIColor.blue,
    UIColor.green,
    UIColor.magenta,
    UIColor.orange,
    UIColor.purple,
    UIColor.red,
    UIColor.yellow,
  ]

  override func viewDidLoad() {
    super.viewDidLoad()

    minimumLoadSlider.valueFormatter =
      { (value: Float) in return String(Int(value * 100)) + "%" }

    displayLink = CADisplayLink(target: self, selector: #selector(displayLinkStep))
    displayLink.add(to: .main, forMode: RunLoop.Mode.common)

    let extraBufferingDuration = ViewController.activityViewLatency * 2
    driveDurationsView!.duration = ViewController.activityViewDuration
    driveDurationsView!.extraBufferingDuration = extraBufferingDuration
    driveDurationsView!.missingTimeColor = ViewController.dropoutColor

    for i in 0..<numProcessors() {
      let coreActivityView = ActivityView.init(frame: .zero)
      coreActivityView.duration = ViewController.activityViewDuration
      coreActivityView.extraBufferingDuration = extraBufferingDuration
      coreActivityView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      coreActivityViews.append(coreActivityView)

      let label = UILabel.init(frame: CGRect(x: 0, y: 0, width: 9, height: 0))
      label.textAlignment = .center
      label.font = UIFont.systemFont(ofSize: 9)
      label.text = String(i + 1)
      label.autoresizingMask = [.flexibleHeight]
      label.backgroundColor = UIColor.white.withAlphaComponent(0.5)

      let rowForCore = UIView.init(frame: .zero)
      rowForCore.addSubview(coreActivityView)
      rowForCore.addSubview(label)
      coreActivityStackView.addArrangedSubview(rowForCore)
    }

    engine = Engine()
    updateAllControls()
    updateActivityViewState()
  }

  private func updateAllControls() {
    bufferSizeStepper.value = log2(Double(engine.preferredBufferSize))
    bufferSizeField.text = String(engine.preferredBufferSize)
    numSinesSlider.value = Float(engine.numSines)
    numSinesSlider.maximumValue = Float(engine.maxNumSines)
    numBurstSinesSlider.maximumValue = Float(engine.maxNumSines)
    numThreadsSlider.value = Float(engine.numWorkerThreads + 1)
    minimumLoadSlider.value = Float(engine.minimumLoad)
    numBusyThreadsSlider.value = Float(engine.numBusyThreads)
    isWorkIntervalOnSwitch.isOn = engine.isWorkIntervalOn
    updateWorkIntervalEnabledState()
  }

  private func updateWorkIntervalEnabledState() {
    isWorkIntervalOnSwitch.isEnabled = engine.numWorkerThreads > 0
  }

  @IBAction private func activityViewsEnabledChanged(_ sender: Any) {
    updateActivityViewState()
  }

  @IBAction private func bufferSizeChanged(_ sender: Any) {
    engine.preferredBufferSize = 1 << Int(bufferSizeStepper.value)
    bufferSizeField.text = String(engine.preferredBufferSize)
  }

  @IBAction private func numSinesChanged(_ sender: Any) {
    engine.numSines = Int32(numSinesSlider!.value)
  }

  @IBAction private func numThreadsChanged(_ sender: Any) {
    engine.numWorkerThreads = Int32(numThreadsSlider!.value) - 1
    updateWorkIntervalEnabledState()
  }

  @IBAction private func minimumLoadChanged(_ sender: Any) {
    engine.minimumLoad = Double(minimumLoadSlider!.value)
  }

  @IBAction private func numBusyThreadsChanged(_ sender: Any) {
    engine.numBusyThreads = Int32(numBusyThreadsSlider!.value)
  }

  @IBAction private func isWorkIntervalOnChanged(_ sender: Any) {
    engine.isWorkIntervalOn = isWorkIntervalOnSwitch.isOn
  }

  @IBAction private func playSineBurst(_ sender: Any) {
    engine.playSineBurst(for: 0.25, additionalSines: Int32(numBurstSinesSlider.value))
  }

  private func updateActivityViewState() {
    displayLink.isPaused = activityViewsEnabledSwitch.isOn == false
  }

  static private func getThreadIndexPerCpu(fromMeasurement measurement: DriveMeasurement)
    -> Array<Int?> {
    var threadIndexPerCpu = Array<Int?>(repeating: nil, count: numProcessors())
    var threadIndex = 0
    for reflectedCpuNum in Mirror(reflecting: measurement.cpuNumbers).children {
      let cpuNum = Int(reflectedCpuNum.value as! Int32)
      if cpuNum >= 0 {
        threadIndexPerCpu[cpuNum] = threadIndex
      }
      threadIndex = threadIndex + 1
    }
    return threadIndexPerCpu
  }

  private func fetchDriveMeasurements() {
    engine.fetchMeasurements({(measurement: DriveMeasurement) in
      if measurement.numFrames != self.lastNumFrames {
        os_log("Actual Buffer Size: %d", measurement.numFrames)
        self.lastNumFrames = measurement.numFrames
      }

      let numFramesInSeconds = Double(measurement.numFrames) / self.engine.sampleRate
      let driveStartTime = measurement.hostTime - numFramesInSeconds
      let color = measurement.duration <= numFramesInSeconds
        ? UIColor.black : ViewController.dropoutColor
      self.driveDurationsView.addSample(
          time: driveStartTime,
          duration: numFramesInSeconds,
          value: measurement.duration / numFramesInSeconds,
          color: color)

      let threadIndexPerCpu = ViewController.getThreadIndexPerCpu(
        fromMeasurement: measurement)
      var cpuNumber = 0
      for coreActivityView in self.coreActivityViews {
        let threadIndex = threadIndexPerCpu[cpuNumber]
        let color = threadIndex != nil
          ? ViewController.threadColors[threadIndex!] : UIColor.white
        coreActivityView.addSample(
          time: driveStartTime,
          duration: numFramesInSeconds,
          value: threadIndex != nil ? 1.0 : 0.0,
          color: color)
        cpuNumber = cpuNumber + 1
      }
    })
  }

  @objc private func displayLinkStep(displaylink: CADisplayLink) {
    fetchDriveMeasurements()

    let activityViewStartTime = displayLink.timestamp -
      ViewController.activityViewDuration - ViewController.activityViewLatency
    self.driveDurationsView.startTime = activityViewStartTime
    self.driveDurationsView.setNeedsDisplay()
    for coreActivityView in self.coreActivityViews {
      coreActivityView.startTime = activityViewStartTime
      coreActivityView.setNeedsDisplay()
    }
  }
}
