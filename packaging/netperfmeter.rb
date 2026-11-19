class Netperfmeter < Formula
  desc "Network Performance Meter"
  homepage "https://www.nntb.no/~dreibh/netperfmeter/"
  url "https://www.nntb.no/~dreibh/netperfmeter/download/netperfmeter-2.0.14.tar.xz"
  sha256 "0c24e2a3fded88570b42cef9d8de92e10210b22347f2f459a40d3686a0fced39"
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
  depends_on "qpdf" if build.with? "plot-programs"
  depends_on "r" if build.with?("example-scripts") || build.with?("plot-programs")
  depends_on "shared-mime-info" if build.with? "icons"

  uses_from_macos "bzip2"
  uses_from_macos "perl"

  # Perl CPAN dependencies for setpdfmetadata (PDF::API2 -> Font::TTF)
  resource "Font::TTF" do
    url "https://www.cpan.org/modules/by-module/Font/Font-TTF-1.06.tar.gz"
    sha256 "4b697d444259759ea02d2c442c9bffe5ffe14c9214084a01f743693a944cc293"
  end

  resource "PDF::API2" do
    url "https://www.cpan.org/modules/by-module/PDF/PDF-API2-2.048.tar.gz"
    sha256 "369de1a4a5652899a39a45598326248d9c72f02c16812a50e868b7401f634d89"
  end

  def install
    if build.with? "plot-programs"
      ENV.prepend_create_path "PERL5LIB", libexec/"perl5/lib/perl5"
      %w[Font::TTF PDF::API2].each do |r|
        resource(r).stage do
          system "perl", "Makefile.PL", "INSTALL_BASE=#{libexec}/perl5"
          system "make"
          system "make", "install"
        end
      end
    end

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

    if build.with? "plot-programs"
      (libexec/"bin").install bin/"setpdfmetadata"
      (bin/"setpdfmetadata").write_env_script libexec/"bin/setpdfmetadata",
        PERL5LIB: "#{libexec}/perl5/lib/perl5"

      (libexec/"bin").install bin/"plot-netperfmeter-results"
      (bin/"plot-netperfmeter-results").write_env_script libexec/"bin/plot-netperfmeter-results",
        R_LIBS_SITE: "#{libexec}/r-library"
    end
  end

  def post_install
    if build.with?("example-scripts") || build.with?("plot-programs")
      r_lib = libexec/"r-library"
      r_lib.mkpath
      system Formula["r"].opt_bin/"Rscript", "-e",
        "install.packages(c('data.table', 'dplyr', 'ggplot2'), " \
        "lib='#{r_lib}', repos='https://cloud.r-project.org')"
    end
  end

  test do
    system bin/"netperfmeter", "--version"
    if build.with? "plot-programs"
      system bin/"setpdfmetadata", "--help"
      system Formula["r"].opt_bin/"Rscript", "-e",
        ".libPaths(c('#{libexec}/r-library', .libPaths())); " \
        "library(data.table); library(dplyr); library(ggplot2)"
    end
  end
end
