class Netperfmeter < Formula
  desc "Network Performance Meter"
  homepage "https://www.nntb.no/~dreibh/netperfmeter/"
  url "https://www.nntb.no/~dreibh/netperfmeter/download/netperfmeter-2.0.11.tar.xz"
  sha256 "2ef94e568767dcd4646f0995dc38a9dd5f155b043d9c05054022b0026f01a70a"
  license "GPL-3.0-or-later"

  # Options ON by default (matching FreeBSD OPTIONS_DEFAULT)
  option "without-example-results", "Build without example results"
  option "without-example-scripts", "Build without example scripts"
  option "without-icons", "Build without icon files"
  option "without-plot-programs", "Build without plot programs"

  # Build-time dependencies
  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build

  # Feature dependencies (strictly sorted alphabetically)
  depends_on "ghostscript" if build.with? "icons"
  depends_on "graphicsmagick" if build.with? "icons"
  depends_on "pdf2svg" if build.with? "icons"
  depends_on "r" if build.with? "plot-programs"
  depends_on "shared-mime-info" if build.with? "icons"

  def install
    args = std_cmake_args + %W[
      -GNinja
      -DCMAKE_MACOSX_RPATH=ON
      -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
      -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=ON
      -DCMAKE_INSTALL_RPATH=#{rpath}
      -DWITH_EXAMPLE_RESULTS=#{build.with?("example-results") ? "ON" : "OFF"}
      -DWITH_EXAMPLE_SCRIPTS=#{build.with?("example-scripts") ? "ON" : "OFF"}
      -DWITH_ICONS=#{build.with?("icons") ? "ON" : "OFF"}
      -DWITH_PLOT_PROGRAMS=#{build.with?("plot-programs") ? "ON" : "OFF"}
    ]

    system "cmake", "-S", ".", "-B", "build", *args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    system "#{bin}/netperfmeter", "--version"
  end
end
