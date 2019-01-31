// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

import UIKit
import os

class ViewController: UITableViewController {
  private var engine = Engine()
  private var displayLink: CADisplayLink?
  private var coreActivityViews: [ActivityView] = []
  private var tableViewHeaders: [CollapsibleTableViewHeader] = []
  private var lastNumFrames: Int32?

  private var lastEnergyUsageTime: Double?
  private var lastEnergyUsage: Double?
  private var lastPowerLabelUpdateTime: Double?
  private var lastEnergyUsageForPowerLabel: Double?

  @IBOutlet weak private var activityViewsEnabledSwitch: UISwitch!
  @IBOutlet weak private var driveDurationsView: ActivityView!
  @IBOutlet weak private var coreActivityStackView: UIStackView!
  @IBOutlet weak private var energyUsageView: ActivityView!
  @IBOutlet weak private var bufferSizeStepper: UIStepper!
  @IBOutlet weak private var bufferSizeField: UITextField!
  @IBOutlet weak private var numSinesSlider: SliderWithValue!
  @IBOutlet weak private var numBurstSinesSlider: SliderWithValue!
  @IBOutlet weak private var numProcessingThreadsSlider: SliderWithValue!
  @IBOutlet weak private var minimumLoadSlider: SliderWithValue!
  @IBOutlet weak private var numBusyThreadsSlider: SliderWithValue!
  @IBOutlet weak private var processInDriverThreadControl: UISegmentedControl!
  @IBOutlet weak private var isWorkIntervalOnSwitch: UISwitch!

  private static let maxEnergyViewPowerInWatts = 5.0
  private static let powerLabelUpdateInterval = 0.5

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

    for section in 0..<tableView.numberOfSections {
      let title = tableView(tableView, titleForHeaderInSection: section)!
      tableViewHeaders.append(makeTableViewHeader(title: title))
    }
    tableViewHeader("Cores")!.isExpanded = false
    tableViewHeader("Energy")!.isExpanded = false

    minimumLoadSlider.valueFormatter = { (value: Float) in return "\(Int(value * 100))%"}

    displayLink = CADisplayLink(target: self, selector: #selector(displayLinkStep))
    displayLink!.add(to: .main, forMode: RunLoop.Mode.common)

    let extraBufferingDuration = ViewController.activityViewLatency * 2
    driveDurationsView.duration = ViewController.activityViewDuration
    driveDurationsView.extraBufferingDuration = extraBufferingDuration
    driveDurationsView.missingTimeColor = ViewController.dropoutColor

    for i in 0..<numberOfProcessors {
      let coreActivityView = ActivityView(frame: .zero)
      coreActivityView.duration = ViewController.activityViewDuration
      coreActivityView.extraBufferingDuration = extraBufferingDuration
      coreActivityView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      coreActivityView.contentMode = .scaleAspectFit
      coreActivityViews.append(coreActivityView)

      let label = UILabel(frame: CGRect(x: 0, y: 0, width: 9, height: 0))
      label.textAlignment = .center
      label.font = UIFont.systemFont(ofSize: 9)
      label.text = String(i + 1)
      label.autoresizingMask = [.flexibleHeight]
      label.backgroundColor = UIColor.white.withAlphaComponent(0.5)

      let rowForCore = UIView(frame: .zero)
      rowForCore.addSubview(coreActivityView)
      rowForCore.addSubview(label)
      coreActivityStackView.addArrangedSubview(rowForCore)
    }

    energyUsageView.duration = ViewController.activityViewDuration
    energyUsageView.extraBufferingDuration = extraBufferingDuration

    initalizeControls()
  }

  private func initalizeControls() {
    bufferSizeStepper.value = log2(Double(engine.preferredBufferSize))
    bufferSizeField.text = String(engine.preferredBufferSize)
    numSinesSlider.value = Float(engine.numSines)
    numSinesSlider.minimumValue = Float(engine.numSines)
    numSinesSlider.maximumValue = Float(engine.maxNumSines)
    numBurstSinesSlider.maximumValue = Float(engine.maxNumSines)
    numProcessingThreadsSlider.value =
      Float(engine.numWorkerThreads + (engine.processInDriverThread ? 1 : 0))
    minimumLoadSlider.value = Float(engine.minimumLoad)
    numBusyThreadsSlider.value = Float(engine.numBusyThreads)
    processInDriverThreadControl.selectedSegmentIndex =
      engine.processInDriverThread ? 1 : 0
    isWorkIntervalOnSwitch.isOn = engine.isWorkIntervalOn
    updateWorkIntervalEnabledState()
  }

  private func updateWorkIntervalEnabledState() {
    isWorkIntervalOnSwitch.isEnabled = engine.numWorkerThreads > 0
  }

  private func updateNumEngineWorkerThreads() {
    engine.numWorkerThreads =
      Int32(numProcessingThreadsSlider.value) - (engine.processInDriverThread ? 1 : 0)
    updateWorkIntervalEnabledState()
  }

  @IBAction private func bufferSizeChanged(_ sender: Any) {
    engine.preferredBufferSize = 1 << Int(bufferSizeStepper.value)
    bufferSizeField.text = String(engine.preferredBufferSize)
  }

  @IBAction private func numSinesChanged(_ sender: Any) {
    engine.numSines = Int32(numSinesSlider!.value)
  }

  @IBAction private func numProcessingThreadsChanged(_ sender: Any) {
    updateNumEngineWorkerThreads()
  }

  @IBAction private func minimumLoadChanged(_ sender: Any) {
    engine.minimumLoad = Double(minimumLoadSlider!.value)
  }

  @IBAction private func numBusyThreadsChanged(_ sender: Any) {
    engine.numBusyThreads = Int32(numBusyThreadsSlider!.value)
  }

  @IBAction private func processInDriverThreadChanged(_ sender: Any) {
    engine.processInDriverThread = processInDriverThreadControl.selectedSegmentIndex == 1
    updateNumEngineWorkerThreads()
  }

  @IBAction private func isWorkIntervalOnChanged(_ sender: Any) {
    engine.isWorkIntervalOn = isWorkIntervalOnSwitch.isOn
  }

  @IBAction private func playSineBurst(_ sender: Any) {
    engine.playSineBurst(for: 0.25, additionalSines: Int32(numBurstSinesSlider.value))
  }

  func makeTableViewHeader(title: String) -> CollapsibleTableViewHeader {
    let headerView = CollapsibleTableViewHeader(frame: .zero)
    headerView.title = title
    headerView.onTap = {
      self.tableView.beginUpdates()
      self.tableView.endUpdates()
    }
    return headerView
  }

  func tableViewHeader(_ title: String) -> CollapsibleTableViewHeader? {
    return tableViewHeaders.first(where: { $0.title == title })
  }

  override func tableView(
    _ tableView: UITableView,
    heightForHeaderInSection section: Int) -> CGFloat {
    return 40.0
  }

  override func tableView(
    _ tableView: UITableView,
    heightForRowAt indexPath: IndexPath) -> CGFloat {
    if tableViewHeaders[indexPath.section].isExpanded {
      return super.tableView(tableView, heightForRowAt: indexPath)
    } else {
      // Keep a separator line for collapsed sections
      return indexPath.row == 0 ? 0.5 : 0.0
    }
  }

  override func tableView(
    _ tableView: UITableView,
    viewForHeaderInSection section: Int) -> UIView? {
    return tableViewHeaders[section]
  }

  static private func getThreadIndexPerCpu(from measurement: DriveMeasurement) -> [Int?] {
    var threadIndexPerCpu = [Int?](repeating: nil, count: numberOfProcessors)
    for (threadIndex, reflectedCpuNum) in
      Mirror(reflecting: measurement.cpuNumbers).children.enumerated() {
      let cpuNum = Int(reflectedCpuNum.value as! Int32)
      if cpuNum >= 0 {
        threadIndexPerCpu[cpuNum] = threadIndex
      }
    }
    return threadIndexPerCpu
  }

  private func fetchDriveMeasurements() {
    engine.fetchMeasurements({ measurement in
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

      let threadIndexPerCpu = ViewController.getThreadIndexPerCpu(from: measurement)
      for (cpuNumber, coreActivityView) in self.coreActivityViews.enumerated() {
        let threadIndex = threadIndexPerCpu[cpuNumber]
        let color = threadIndex != nil
          ? ViewController.threadColors[threadIndex!] : UIColor.white
        coreActivityView.addSample(
          time: driveStartTime,
          duration: numFramesInSeconds,
          value: threadIndex != nil ? 1.0 : 0.0,
          color: color)
      }
    })
  }

  private func fetchPowerMeasurements() {
    let energyHeader = tableViewHeader("Energy")!
    guard let energyUsage = taskEnergyUsage else {
      energyHeader.value = "Not Available"
      return
    }

    let time = CACurrentMediaTime()

    if let lastEnergyUsageTime = lastEnergyUsageTime,
       let lastEnergyUsage = lastEnergyUsage {
      let timeDelta = time - lastEnergyUsageTime
      let powerInWatts = (energyUsage - lastEnergyUsage) / timeDelta
      let normalizedPower = powerInWatts / ViewController.maxEnergyViewPowerInWatts
      energyUsageView.addSample(
        time: lastEnergyUsageTime,
        duration: timeDelta,
        value: normalizedPower,
        color: UIColor.black)
    }

    if (lastPowerLabelUpdateTime == nil
      || (time - lastPowerLabelUpdateTime!) > ViewController.powerLabelUpdateInterval) {
      if let lastPowerLabelUpdateTime = lastPowerLabelUpdateTime,
         let lastEnergyUsageForPowerLabel = lastEnergyUsageForPowerLabel {
        let timeDelta = time - lastPowerLabelUpdateTime
        let powerInWatts = (energyUsage - lastEnergyUsageForPowerLabel) / timeDelta
        energyHeader.value = String(format: "%.2f Watts", powerInWatts)
      }

      lastPowerLabelUpdateTime = time
      lastEnergyUsageForPowerLabel = energyUsage
    }

    lastEnergyUsageTime = time
    lastEnergyUsage = energyUsage
  }

  @objc private func displayLinkStep(displayLink: CADisplayLink) {
    fetchDriveMeasurements()
    fetchPowerMeasurements()

    if activityViewsEnabledSwitch.isOn {
      let activityViewStartTime = displayLink.timestamp -
        ViewController.activityViewDuration - ViewController.activityViewLatency

      if tableViewHeader("Load")!.isExpanded {
        driveDurationsView.startTime = activityViewStartTime
        driveDurationsView.setNeedsDisplay()
      }
      if tableViewHeader("Cores")!.isExpanded {
        for coreActivityView in coreActivityViews {
          coreActivityView.startTime = activityViewStartTime
          coreActivityView.setNeedsDisplay()
        }
      }
      if tableViewHeader("Energy")!.isExpanded {
        energyUsageView.startTime = activityViewStartTime
        energyUsageView.setNeedsDisplay()
      }
    }
  }
}

fileprivate var numberOfProcessors: Int {
  return ProcessInfo.processInfo.processorCount
}

// Energy usage of the current task in joules
fileprivate var taskEnergyUsage: Double? {
#if arch(arm) || arch(arm64)
  let TASK_POWER_INFO_V2_COUNT =
    MemoryLayout<task_power_info_v2>.stride/MemoryLayout<natural_t>.stride
  var info = task_power_info_v2()
  var count = mach_msg_type_number_t(TASK_POWER_INFO_V2_COUNT)

  let kerr = withUnsafeMutablePointer(to: &info) {
    $0.withMemoryRebound(to: integer_t.self, capacity: TASK_POWER_INFO_V2_COUNT) {
        task_info(mach_task_self_,
                  task_flavor_t(TASK_POWER_INFO_V2),
                  $0,
                  &count)
    }
  }

  return kerr == KERN_SUCCESS ? Double(info.task_energy) * 1.0e-9 : nil
#else
  return nil
#endif
}
